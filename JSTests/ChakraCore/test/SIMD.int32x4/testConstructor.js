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

function testConstructor() {
    print("Constructor");
    equal(SIMD.Int32x4, undefined);
    equal(SIMD.Int32x4(1, 2, 3, 4), undefined);
    var a = SIMD.Int32x4("2014/10/10", -0, 2147483648, 2147483647);
    print("a.x: " + SIMD.Int32x4.extractLane(a, 0));
    print("a.y: " + SIMD.Int32x4.extractLane(a, 1));
    print("a.z: " + SIMD.Int32x4.extractLane(a, 2));
    print("a.w: " + SIMD.Int32x4.extractLane(a, 3));
    var b = SIMD.Int32x4(4, 3, 2, 1);
    var c = SIMD.Int32x4.check(b);
    equal(c, b);
    equal(SIMD.Int32x4.extractLane(c, 0), SIMD.Int32x4.extractLane(b, 0));
    equal(SIMD.Int32x4.extractLane(c, 1), SIMD.Int32x4.extractLane(b, 1));
    equal(SIMD.Int32x4.extractLane(c, 2), SIMD.Int32x4.extractLane(b, 2));
    equal(SIMD.Int32x4.extractLane(c, 3), SIMD.Int32x4.extractLane(b, 3));

    try {
        var m = SIMD.Int32x4.check(1)
    }
    catch (e) {
        print("Type Error");
    }
}

function testFromFloat32x4() {
    var m = SIMD.Float32x4(1.0, 2.2, 3.6, 4.8);
    var n = SIMD.Int32x4.fromFloat32x4(m);
    print("FromFloat32x4");
    equal(1, SIMD.Int32x4.extractLane(n, 0));
    equal(2, SIMD.Int32x4.extractLane(n, 1));
    equal(3, SIMD.Int32x4.extractLane(n, 2));
    equal(4, SIMD.Int32x4.extractLane(n, 3));
}

function testFromFloat32x4Bits() {
    var m = SIMD.Float32x4.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromFloat32x4Bits(m);
    print("FromFloat32x4Bits");
    equal(1, SIMD.Int32x4.extractLane(n, 0));
    equal(2, SIMD.Int32x4.extractLane(n, 1));
    equal(3, SIMD.Int32x4.extractLane(n, 2));
    equal(4, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    var b = SIMD.Int32x4.fromFloat32x4Bits(a);
    equal(0x3F800000, SIMD.Int32x4.extractLane(b, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(b, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(b, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(b, 3));
}

function testFromInt8x16Bits() {
    var m = SIMD.Int8x16.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromInt8x16Bits(m);
    WScript.Echo("FromInt8x16Bits");
    equal(0x3F800000, SIMD.Int32x4.extractLane(n, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(n, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(n, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8);
    var b = SIMD.Int32x4.fromInt8x16Bits(a);
    equal(0x04030201, SIMD.Int32x4.extractLane(b, 0));
    equal(0x08070605, SIMD.Int32x4.extractLane(b, 1));
    equal(0x00000000, SIMD.Int32x4.extractLane(b, 2));
    equal(0x00000000, SIMD.Int32x4.extractLane(b, 3));
    var c = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    var d = SIMD.Int32x4.fromInt8x16Bits(c);
    equal(0x04030201, SIMD.Int32x4.extractLane(d, 0));
    equal(0x08070605, SIMD.Int32x4.extractLane(d, 1));
    equal(0x0C0B0A09, SIMD.Int32x4.extractLane(d, 2));
    equal(0x000F0E0D, SIMD.Int32x4.extractLane(d, 3));
}

function testFromInt16x8Bits() {
    var m = SIMD.Int16x8.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromInt16x8Bits(m);
    WScript.Echo("FromInt16x8Bits");
    equal(0x3F800000, SIMD.Int32x4.extractLane(n, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(n, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(n, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Int16x8(1, 2, 3, 4, 5, 6, 7, 8);
    var b = SIMD.Int32x4.fromInt16x8Bits(a);
    equal(0x00020001, SIMD.Int32x4.extractLane(b, 0));
    equal(0x00040003, SIMD.Int32x4.extractLane(b, 1));
    equal(0x00060005, SIMD.Int32x4.extractLane(b, 2));
    equal(0x00080007, SIMD.Int32x4.extractLane(b, 3));
}

function testFromUint32x4Bits() {
    var m = SIMD.Uint32x4.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromUint32x4Bits(m);
    WScript.Echo("FromUint32x4Bits");
    equal(0x3F800000, SIMD.Int32x4.extractLane(n, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(n, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(n, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Uint32x4(1, 2, 3, 4);
    var b = SIMD.Int32x4.fromUint32x4Bits(a);
    equal(0x00000001, SIMD.Int32x4.extractLane(b, 0));
    equal(0x00000002, SIMD.Int32x4.extractLane(b, 1));
    equal(0x00000003, SIMD.Int32x4.extractLane(b, 2));
    equal(0x00000004, SIMD.Int32x4.extractLane(b, 3));
}

function testFromUint16x8Bits() {
    var m = SIMD.Uint16x8.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromUint16x8Bits(m);
    WScript.Echo("FromUint16x8Bits");
    equal(0x3F800000, SIMD.Int32x4.extractLane(n, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(n, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(n, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Uint16x8(1, 2, 3, 4, 5, 6, 7, 8);
    var b = SIMD.Int32x4.fromUint16x8Bits(a);
    equal(0x00020001, SIMD.Int32x4.extractLane(b, 0));
    equal(0x00040003, SIMD.Int32x4.extractLane(b, 1));
    equal(0x00060005, SIMD.Int32x4.extractLane(b, 2));
    equal(0x00080007, SIMD.Int32x4.extractLane(b, 3));
}

function testFromUint8x16Bits() {
    var m = SIMD.Uint8x16.fromInt32x4Bits(SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000));
    var n = SIMD.Int32x4.fromUint8x16Bits(m);
    WScript.Echo("FromUint8x16Bits");
    equal(0x3F800000, SIMD.Int32x4.extractLane(n, 0));
    equal(0x40000000, SIMD.Int32x4.extractLane(n, 1));
    equal(0x40400000, SIMD.Int32x4.extractLane(n, 2));
    equal(0x40800000, SIMD.Int32x4.extractLane(n, 3));
    var a = SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var b = SIMD.Int32x4.fromUint8x16Bits(a);
    equal(0x04030201, SIMD.Int32x4.extractLane(b, 0));
    equal(0x08070605, SIMD.Int32x4.extractLane(b, 1));
    equal(0x0C0B0A09, SIMD.Int32x4.extractLane(b, 2));
    equal(0x100F0E0D, SIMD.Int32x4.extractLane(b, 3));
}

testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();


testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();

testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();

testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();

testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();

testConstructor();
testFromFloat32x4();
testFromFloat32x4Bits();

testFromInt8x16Bits();
testFromInt8x16Bits();
testFromInt8x16Bits();

testFromInt16x8Bits();
testFromInt16x8Bits();
testFromInt16x8Bits();

testFromUint32x4Bits();
testFromUint32x4Bits();
testFromUint32x4Bits();

testFromUint16x8Bits();
testFromUint16x8Bits();
testFromUint16x8Bits();

testFromUint8x16Bits();
testFromUint8x16Bits();
testFromUint8x16Bits();

