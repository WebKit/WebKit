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

function testShiftleftByScalar() {
    print("Int8x16 shiftLeftByScalar");
    // var a = SIMD.Int8x16(0x80000000, 0x7000000, 0xFFFFFFFF, 0x0);
    var a = SIMD.Int8x16(1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10);
    var b = SIMD.Int8x16.shiftLeftByScalar(a, 1)
    equal(0x2, SIMD.Int8x16.extractLane(b, 0));
    equal(0x4, SIMD.Int8x16.extractLane(b, 1));
    equal(0x6, SIMD.Int8x16.extractLane(b, 2));
    equal(0x8, SIMD.Int8x16.extractLane(b, 3));
    equal(0xA, SIMD.Int8x16.extractLane(b, 4));
    equal(0xC, SIMD.Int8x16.extractLane(b, 5));
    equal(0xE, SIMD.Int8x16.extractLane(b, 6));
    equal(0x10, SIMD.Int8x16.extractLane(b, 7));
    equal(0x12, SIMD.Int8x16.extractLane(b, 8));
    equal(0x14, SIMD.Int8x16.extractLane(b, 9));
    equal(0x16, SIMD.Int8x16.extractLane(b, 10));
    equal(0x18, SIMD.Int8x16.extractLane(b, 11));
    equal(0x1A, SIMD.Int8x16.extractLane(b, 12));
    equal(0x1C, SIMD.Int8x16.extractLane(b, 13));
    equal(0x1E, SIMD.Int8x16.extractLane(b, 14));
    equal(0x20, SIMD.Int8x16.extractLane(b, 15));

    var c = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var d = SIMD.Int8x16.shiftLeftByScalar(c, 1)
    equal(2, SIMD.Int8x16.extractLane(d, 0));
    equal(4, SIMD.Int8x16.extractLane(d, 1));
    equal(6, SIMD.Int8x16.extractLane(d, 2));
    equal(8, SIMD.Int8x16.extractLane(d, 3));
    equal(10, SIMD.Int8x16.extractLane(d, 4));
    equal(12, SIMD.Int8x16.extractLane(d, 5));
    equal(14, SIMD.Int8x16.extractLane(d, 6));
    equal(16, SIMD.Int8x16.extractLane(d, 7));
    equal(18, SIMD.Int8x16.extractLane(d, 8));
    equal(20, SIMD.Int8x16.extractLane(d, 9));
    equal(22, SIMD.Int8x16.extractLane(d, 10));
    equal(24, SIMD.Int8x16.extractLane(d, 11));
    equal(26, SIMD.Int8x16.extractLane(d, 12));
    equal(28, SIMD.Int8x16.extractLane(d, 13));
    equal(30, SIMD.Int8x16.extractLane(d, 14));
    equal(32, SIMD.Int8x16.extractLane(d, 15));

}


function testShiftRightByScalar() {
    print("Int8x16 shiftRightByScalar");
    var a = SIMD.Int8x16(0x80, 0x70, 0xFF, 0x0, 0x80, 0x70, 0xFF, 0x0, 0x80, 0x70, 0xFF, 0x0, 0x80, 0x70, 0xFF, 0x0);
    var b = SIMD.Int8x16.shiftRightByScalar(a, 1);

    equal(-64, SIMD.Int8x16.extractLane(b, 0));
    equal(56, SIMD.Int8x16.extractLane(b, 1));
    equal(-1, SIMD.Int8x16.extractLane(b, 2));
    equal(0x0, SIMD.Int8x16.extractLane(b, 3));
    equal(-64, SIMD.Int8x16.extractLane(b, 4));
    equal(56, SIMD.Int8x16.extractLane(b, 5));
    equal(-1, SIMD.Int8x16.extractLane(b, 6));
    equal(0x0, SIMD.Int8x16.extractLane(b, 7));
    equal(-64, SIMD.Int8x16.extractLane(b, 8));
    equal(56, SIMD.Int8x16.extractLane(b, 9));
    equal(-1, SIMD.Int8x16.extractLane(b, 10));
    equal(0x0, SIMD.Int8x16.extractLane(b, 11));
    equal(-64, SIMD.Int8x16.extractLane(b, 12));
    equal(56, SIMD.Int8x16.extractLane(b, 13));
    equal(-1, SIMD.Int8x16.extractLane(b, 14));
    equal(0x0, SIMD.Int8x16.extractLane(b, 15));

    var c = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var d = SIMD.Int8x16.shiftRightByScalar(c, 1)
    equal(0, SIMD.Int8x16.extractLane(d, 0));
    equal(1, SIMD.Int8x16.extractLane(d, 1));
    equal(1, SIMD.Int8x16.extractLane(d, 2));
    equal(2, SIMD.Int8x16.extractLane(d, 3));
    equal(2, SIMD.Int8x16.extractLane(d, 4));
    equal(3, SIMD.Int8x16.extractLane(d, 5));
    equal(3, SIMD.Int8x16.extractLane(d, 6));
    equal(4, SIMD.Int8x16.extractLane(d, 7));
    equal(4, SIMD.Int8x16.extractLane(d, 8));
    equal(5, SIMD.Int8x16.extractLane(d, 9));
    equal(5, SIMD.Int8x16.extractLane(d, 10));
    equal(6, SIMD.Int8x16.extractLane(d, 11));
    equal(6, SIMD.Int8x16.extractLane(d, 12));
    equal(7, SIMD.Int8x16.extractLane(d, 13));
    equal(7, SIMD.Int8x16.extractLane(d, 14));
    equal(8, SIMD.Int8x16.extractLane(d, 15));
    var c = SIMD.Int8x16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16);
    var d = SIMD.Int8x16.shiftRightByScalar(c, 1)
    equal(-1, SIMD.Int8x16.extractLane(d, 0));
    equal(-1, SIMD.Int8x16.extractLane(d, 1));
    equal(-2, SIMD.Int8x16.extractLane(d, 2));
    equal(-2, SIMD.Int8x16.extractLane(d, 3));
    equal(-3, SIMD.Int8x16.extractLane(d, 4));
    equal(-3, SIMD.Int8x16.extractLane(d, 5));
    equal(-4, SIMD.Int8x16.extractLane(d, 6));
    equal(-4, SIMD.Int8x16.extractLane(d, 7));
    equal(-5, SIMD.Int8x16.extractLane(d, 8));
    equal(-5, SIMD.Int8x16.extractLane(d, 9));
    equal(-6, SIMD.Int8x16.extractLane(d, 10));
    equal(-6, SIMD.Int8x16.extractLane(d, 11));
    equal(-7, SIMD.Int8x16.extractLane(d, 12));
    equal(-7, SIMD.Int8x16.extractLane(d, 13));
    equal(-8, SIMD.Int8x16.extractLane(d, 14));
    equal(-8, SIMD.Int8x16.extractLane(d, 15));
}

testShiftleftByScalar();
testShiftleftByScalar();
testShiftleftByScalar();
testShiftleftByScalar();

testShiftRightByScalar();
testShiftRightByScalar();
testShiftRightByScalar();
testShiftRightByScalar();