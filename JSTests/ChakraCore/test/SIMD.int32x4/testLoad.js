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

function testLoad() {
    WScript.Echo("Int32x4 load");

    for (var i = 0; i < bufLanes; i++) buf[i] = i; // Number buffer sequentially.

    WScript.Echo("Int32x4 load: Test aligned loads");
    for (var i = 0; i < 4; i++) {
        var a = SIMD.Int32x4.load(buf, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, i + index);
        }
    }

    WScript.Echo("Int32x4 load: Test 2 possible over-alignments");
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4= lane Size;
    for (var i = 0; i < 1; i++) {
        var a = SIMD.Int32x4.load(f64, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, stride * i + index);
        }
    }

    WScript.Echo("Int32x4 load: Test 7 possible mis-alignments");
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        // Shift the buffer up by 1 byte.
        for (var i = i8.length - 1; i > 0; i--)
            i8[i] = i8[i - 1];
        var a = SIMD.Int32x4.load(i8, misalignment);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, i + index);
        }
    }
}

function testLoad1() {
    WScript.Echo("Int32x4 load1");

    for (var i = 0; i < bufLanes; i++) buf[i] = i; // Number buffer sequentially.

    WScript.Echo("Int32x4 load1: Test aligned loads");
    for (var i = 0; i < 4; i++) {
        var a = SIMD.Int32x4.load1(buf, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 1 ? i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load1: Test 2 possible over-alignments");
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4= type.laneSize;
    for (var i = 0; i < 1; i++) {
        var a = SIMD.Int32x4.load1(f64, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 1 ? stride * i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load1: Test 7 possible mis-alignments");
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        // Shift the buffer up by 1 byte.
        for (var i = i8.length - 1; i > 0; i--)
            i8[i] = i8[i - 1];
        var a = SIMD.Int32x4.load1(i8, misalignment);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 1 ? i + index : 0);
        }
    }
}

function testLoad2() {
    WScript.Echo("Int32x4 load2");

    for (var i = 0; i < bufLanes; i++) buf[i] = i; // Number buffer sequentially.

    WScript.Echo("Int32x4 load2: Test aligned loads");
    for (var i = 0; i < 4; i++) {
        var a = SIMD.Int32x4.load2(buf, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 2 ? i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load2: Test 2 possible over-alignments");
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4= laneSize;
    for (var i = 0; i < 1; i++) {
        var a = SIMD.Int32x4.load2(f64, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 2 ? stride * i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load2: Test 7 possible mis-alignments");
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        // Shift the buffer up by 1 byte.
        for (var i = i8.length - 1; i > 0; i--)
            i8[i] = i8[i - 1];
        var a = SIMD.Int32x4.load2(i8, misalignment);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 2 ? i + index : 0);
        }
    }
}

function testLoad3() {
    WScript.Echo("Int32x4 load3");
    for (var i = 0; i < bufLanes; i++) buf[i] = i; // Number buffer sequentially.

    WScript.Echo("Int32x4 load3: Test aligned loads");
    for (var i = 0; i < 4; i++) {
        var a = SIMD.Int32x4.load3(buf, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 3 ? i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load3: Test 2 possible over-alignments");
    var f64 = new Float64Array(ab);
    var stride = 8 / 4; // 4= laneSize;
    for (var i = 0; i < 1; i++) {
        var a = SIMD.Int32x4.load3(f64, i);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 3 ? stride * i + index : 0);
        }
    }

    WScript.Echo("Int32x4 load3: Test 7 possible mis-alignments");
    var i8 = new Int8Array(ab);
    for (var misalignment = 1; misalignment < 8; misalignment++) {
        // Shift the buffer up by 1 byte.
        for (var i = i8.length - 1; i > 0; i--)
            i8[i] = i8[i - 1];
        var a = SIMD.Int32x4.load3(i8, misalignment);
        for (var index = 0; index < 4; index++) {
            var v = SIMD.Int32x4.extractLane(a, index);
            equal(v, index < 3 ? i + index : 0);
        }
    }
}

testLoad();
testLoad1();
testLoad2();
testLoad3();