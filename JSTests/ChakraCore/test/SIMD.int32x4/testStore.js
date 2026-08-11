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

var bufLanes = 2 * 4; //4 = lane size; Test all alignments.
var bufSize = bufLanes * 4 + 8; //bufLanes * lane Size + 8;  Extra for over-alignment test.

var ab = new ArrayBuffer(bufSize);
var buf = new Int32Array(ab);

function checkBuffer(offset, count) {
    for (var i = 0; i < count; i++) {
        if (buf[offset + i] != i) {
            WScript.Echo(">> Fail!" + "offset + i:" + (offset + i) + "buf:" + buf[offset + i]);
            return false;
        }
    }
    WScript.Echo("Correct");
    return true;
}

function testStore() {
    WScript.Echo("Int32x4 store");

    var ta = new Int32Array(8);
    var value = SIMD.Int32x4(1, 2, 3, 4);
    SIMD.Int32x4.store(ta, 0, value);
    equal(ta[0], 1);
    equal(ta[1], 2);
    equal(ta[2], 3);
    equal(ta[3], 4);

    var a = SIMD.Int32x4(0, 1, 2, 3);

    // Test aligned stores.
    for (var i = 0; i < 4; i++) {
        SIMD.Int32x4.store(buf, i, a);
        checkBuffer(i, 4);
    }

    // Test the 2 over-alignments.
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4 = lane Size;
    for (var i = 0; i < 1; i++) {
        SIMD.Int32x4.store(f64, i, a);
        checkBuffer(stride * i, 4);
    }

    // Test the 7 mis-alignments.
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        SIMD.Int32x4.store(i8, misalignment, a);
        // Shift the buffer down by misalignment.
        for (var i = 0; i < i8.length - misalignment; i++)
            i8[i] = i8[i + misalignment];
        checkBuffer(0, 4);
    }

}

function testStore1() {
    WScript.Echo("Int32x4 store1");

    var ta = new Int32Array(8);
    var value = SIMD.Int32x4(1, 2, 3, 4);
    SIMD.Int32x4.store1(ta, 0, value);
    equal(ta[0], 1);
    equal(ta[1], 0);
    equal(ta[2], 0);
    equal(ta[3], 0);

    var a = SIMD.Int32x4(0, 1, 2, 3);

    // Test aligned stores.
    for (var i = 0; i < 4; i++) {
        SIMD.Int32x4.store1(buf, i, a);
        checkBuffer(i, 1);
    }

    // Test the 2 over-alignments.
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4 = lane Size;
    for (var i = 0; i < 1; i++) {
        SIMD.Int32x4.store1(f64, i, a);
        checkBuffer(stride * i, 1);
    }

    // Test the 7 mis-alignments.
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        SIMD.Int32x4.store1(i8, misalignment, a);
        // Shift the buffer down by misalignment.
        for (var i = 0; i < i8.length - misalignment; i++)
            i8[i] = i8[i + misalignment];
        checkBuffer(0, 1);
    }
}

function testStore2() {
    WScript.Echo("Int32x4 store2");

    var ta = new Int32Array(8);
    var value = SIMD.Int32x4(1, 2, 3, 4);
    SIMD.Int32x4.store2(ta, 0, value);
    equal(ta[0], 1);
    equal(ta[1], 2);
    equal(ta[2], 0);
    equal(ta[3], 0);

    var a = SIMD.Int32x4(0, 1, 2, 3);

    // Test aligned stores.
    for (var i = 0; i < 4; i++) {
        SIMD.Int32x4.store2(buf, i, a);
        checkBuffer(i, 2);
    }

    // Test the 2 over-alignments.
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4 = lane Size;
    for (var i = 0; i < 1; i++) {
        SIMD.Int32x4.store2(f64, i, a);
        checkBuffer(stride * i, 2);
    }

    // Test the 7 mis-alignments.
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        SIMD.Int32x4.store2(i8, misalignment, a);
        // Shift the buffer down by misalignment.
        for (var i = 0; i < i8.length - misalignment; i++)
            i8[i] = i8[i + misalignment];
        checkBuffer(0, 2);
    }
}

function testStore3() {
    WScript.Echo("Int32x4 store3");

    var ta = new Int32Array(8);
    var value = SIMD.Int32x4(1, 2, 3, 4);
    SIMD.Int32x4.store3(ta, 0, value);
    equal(ta[0], 1);
    equal(ta[1], 2);
    equal(ta[2], 3);
    equal(ta[3], 0);

    var a = SIMD.Int32x4(0, 1, 2, 3);

    // Test aligned stores.
    for (var i = 0; i < 4; i++) {
        SIMD.Int32x4.store3(buf, i, a);
        checkBuffer(i, 3);
    }

    // Test the 2 over-alignments.
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4 = lane Size;
    for (var i = 0; i < 1; i++) {
        SIMD.Int32x4.store3(f64, i, a);
        checkBuffer(stride * i, 3);
    }

    // Test the 7 mis-alignments.
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        SIMD.Int32x4.store3(i8, misalignment, a);
        // Shift the buffer down by misalignment.
        for (var i = 0; i < i8.length - misalignment; i++)
            i8[i] = i8[i + misalignment];
        checkBuffer(0, 3);
    }
}

testStore();
testStore1();
testStore2();
testStore3();