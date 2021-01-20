//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
    {
        print("Correct");
    }
    else
    {
        print(">> Fail!");
    }
}

function testComparisons() {
    print("Float32x4 lessThan");
    var m = SIMD.Float32x4(1.0, 2.0, 0.1, 0.001);
    var n = SIMD.Float32x4(2.0, 2.0, 0.001, 0.1);
    var cmp;
    cmp = SIMD.Float32x4.lessThan(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    print("Float32x4 lessThanOrEqual");
    cmp = SIMD.Float32x4.lessThanOrEqual(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    print("Float32x4 equal");
    cmp = SIMD.Float32x4.equal(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 3));

    print("Float32x4 notEqual");
    cmp = SIMD.Float32x4.notEqual(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    print("Float32x4 greaterThanOrEqual");
    cmp = SIMD.Float32x4.greaterThanOrEqual(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 3));

    print("Float32x4 greaterThan");
    cmp = SIMD.Float32x4.greaterThan(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 3));
}

testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();
testComparisons();