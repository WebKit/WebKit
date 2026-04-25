//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");


function testComparisons() {
    var m = SIMD.Int32x4(1000, 2000, 100, 1);
    var n = SIMD.Int32x4(2000, 2000, 1, 100);
    var cmp;
    cmp = SIMD.Int32x4.lessThan(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    cmp = SIMD.Int32x4.equal(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 3));

    cmp = SIMD.Int32x4.greaterThan(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 3));

    cmp = SIMD.Int32x4.lessThanOrEqual(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    cmp = SIMD.Int32x4.notEqual(m, n);
    equal(true, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(false, SIMD.Bool32x4.extractLane(cmp, 1));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 2));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 3));

    cmp = SIMD.Int32x4.greaterThanOrEqual(m, n);
    equal(false, SIMD.Bool32x4.extractLane(cmp, 0));
    equal(true, SIMD.Bool32x4.extractLane(cmp, 1));
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

print("PASS");