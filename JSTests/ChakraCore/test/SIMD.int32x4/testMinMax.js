//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
        WScript.Echo("Correct");
    else
        WScript.Echo(">> Fail!");
}

function dislay(a) {
    WScript.Echo("0:" + SIMD.Int32x4.extractLane(c, 0) + "\n");
    WScript.Echo("1:" + SIMD.Int32x4.extractLane(c, 1) + "\n");
    WScript.Echo("2:" + SIMD.Int32x4.extractLane(c, 2) + "\n");
    WScript.Echo("3:" + SIMD.Int32x4.extractLane(c, 3) + "\n");
}

function testMin() {
    WScript.Echo("Int32x4 min");
    var a = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0x7fffffff, 0x0);
    var b = SIMD.Int32x4(0x1, 0xFFFFFFFF, 0x1, 0xFFFFFFFF);
    var c = SIMD.Int32x4.min(a, b);
    equal(-1, SIMD.Int32x4.extractLane(c, 0));
    equal(-1, SIMD.Int32x4.extractLane(c, 1));
    equal(1, SIMD.Int32x4.extractLane(c, 2));
    equal(-1, SIMD.Int32x4.extractLane(c, 3));

    var m = SIMD.Int32x4(4, 3, 2, 1);
    var n = SIMD.Int32x4(10, 20, 30, 40);
    var f = SIMD.Int32x4.min(m, n);
    equal(4, SIMD.Int32x4.extractLane(f, 0));
    equal(3, SIMD.Int32x4.extractLane(f, 1));
    equal(2, SIMD.Int32x4.extractLane(f, 2));
    equal(1, SIMD.Int32x4.extractLane(f, 3));
}

function testMax() {
    WScript.Echo("Int32x4 max");
    var a = SIMD.Int32x4(0xFFFFFFFF, 0xFFFFFFFF, 0x80000000, 0x0);
    var b = SIMD.Int32x4(0x1, 0xFFFFFFFF, 0x1, 0xFFFFFFFF);
    var c = SIMD.Int32x4.max(a, b);
    equal(1, SIMD.Int32x4.extractLane(c, 0));
    equal(-1, SIMD.Int32x4.extractLane(c, 1));
    equal(1, SIMD.Int32x4.extractLane(c, 2));
    equal(0, SIMD.Int32x4.extractLane(c, 3));

    var d = SIMD.Int32x4(4, 3, 2, 1);
    var e = SIMD.Int32x4(10, 20, 30, 40);
    var f = SIMD.Int32x4.max(d, e);
    equal(10, SIMD.Int32x4.extractLane(f, 0));
    equal(20, SIMD.Int32x4.extractLane(f, 1));
    equal(30, SIMD.Int32x4.extractLane(f, 2));
    equal(40, SIMD.Int32x4.extractLane(f, 3));
}

testMin();
testMin();
testMin();
testMin();
testMin();
testMin();
testMin();

testMax();
testMax();
testMax();
testMax();
testMax();
testMax();
testMax();
