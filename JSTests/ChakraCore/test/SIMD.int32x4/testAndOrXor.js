//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");


function testAnd() {
    var m = SIMD.Int32x4(0xAAAAAAAA, 0xAAAAAAAA, -1431655766, 0xAAAAAAAA);
    var n = SIMD.Int32x4(0x55555555, 0x55555555, 0x55555555, 0x55555555);
    equal(-1431655766, SIMD.Int32x4.extractLane(m, 0));
    equal(-1431655766, SIMD.Int32x4.extractLane(m, 1));
    equal(-1431655766, SIMD.Int32x4.extractLane(m, 2));
    equal(-1431655766, SIMD.Int32x4.extractLane(m, 3));
    equal(0x55555555, SIMD.Int32x4.extractLane(n, 0));
    equal(0x55555555, SIMD.Int32x4.extractLane(n, 1));
    equal(0x55555555, SIMD.Int32x4.extractLane(n, 2));
    equal(0x55555555, SIMD.Int32x4.extractLane(n, 3));

    var o = SIMD.Int32x4.and(m, n);  // and
    equal(0x0, SIMD.Int32x4.extractLane(o, 0));
    equal(0x0, SIMD.Int32x4.extractLane(o, 1));
    equal(0x0, SIMD.Int32x4.extractLane(o, 2));
    equal(0x0, SIMD.Int32x4.extractLane(o, 3));

}

function testOr() {
    var m = SIMD.Int32x4(0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA);
    var n = SIMD.Int32x4(0x55555555, 0x55555555, 0x55555555, 0x55555555);
    var o = SIMD.Int32x4.or(m, n);  // or
    equal(-1, SIMD.Int32x4.extractLane(o, 0));
    equal(-1, SIMD.Int32x4.extractLane(o, 1));
    equal(-1, SIMD.Int32x4.extractLane(o, 2));
    equal(-1, SIMD.Int32x4.extractLane(o, 3));

}

function testXor() {
    var m = SIMD.Int32x4(0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA);
    var n = SIMD.Int32x4(0x55555555, 0x55555555, 0x55555555, 0x55555555);
    n = SIMD.Int32x4.replaceLane(n, 0, 0xAAAAAAAA);
    n = SIMD.Int32x4.replaceLane(n, 1, 0xAAAAAAAA);
    n = SIMD.Int32x4.replaceLane(n, 2, 0xAAAAAAAA);
    n = SIMD.Int32x4.replaceLane(n, 3, 0xAAAAAAAA);
    equal(-1431655766, SIMD.Int32x4.extractLane(n, 0));
    equal(-1431655766, SIMD.Int32x4.extractLane(n, 1));
    equal(-1431655766, SIMD.Int32x4.extractLane(n, 2));
    equal(-1431655766, SIMD.Int32x4.extractLane(n, 3));
    var o = SIMD.Int32x4.xor(m, n);  // xor
    equal(0x0, SIMD.Int32x4.extractLane(o, 0));
    equal(0x0, SIMD.Int32x4.extractLane(o, 1));
    equal(0x0, SIMD.Int32x4.extractLane(o, 2));
    equal(0x0, SIMD.Int32x4.extractLane(o, 3));

}

testAnd();
testAnd();
testAnd();
testAnd();

testOr();
testOr();
testOr();
testOr();

testXor();
testXor();
testXor();
testXor();

print("PASS");