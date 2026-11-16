//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b) {
        print("Correct");
    } else {
        print(">> Fail!");
    }
}

function testComparisons() {
    print("Int8x16 lessThan");
    var m = SIMD.Int8x16(50, 100, 10, 1, 50, 100, 10, 1, 50, 100, 10, 1, 50, 100, 10, 1);
    var n = SIMD.Int8x16(100, 100, 1, 10, 100, 100, 1, 10, 100, 100, 1, 10, 100, 100, 1, 10);
    var cmp;
    cmp = SIMD.Int8x16.lessThan(m, n);

    equal(true, SIMD.Bool8x16.extractLane(cmp, 0));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 1));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 2));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 3));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 4));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 5));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 6));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 7));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 8));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 9));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 10));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 11));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 12));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 13));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 14));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 15));

    print("Int8x16 equal");
    cmp = SIMD.Int8x16.equal(m, n);
    equal(false, SIMD.Bool8x16.extractLane(cmp, 0));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 1));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 2));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 3));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 4));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 5));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 6));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 7));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 8));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 9));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 10));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 11));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 12));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 13));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 14));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 15));

    print("Int8x16 greaterThan");
    cmp = SIMD.Int8x16.greaterThan(m, n);
    equal(false, SIMD.Bool8x16.extractLane(cmp, 0));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 1));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 2));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 3));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 4));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 5));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 6));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 7));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 8));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 9));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 10));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 11));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 12));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 13));
    equal(true, SIMD.Bool8x16.extractLane(cmp, 14));
    equal(false, SIMD.Bool8x16.extractLane(cmp, 15));

}

testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();