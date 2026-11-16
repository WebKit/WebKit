//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16select = i16.select;
    var i16lessThan = i16.lessThan;
    var i16equal = i16.equal;
    var i16greaterThan = i16.greaterThan;
    var i16lessThanOrEqual = i16.lessThanOrEqual;
    var i16greaterThanOrEqual = i16.greaterThanOrEqual;
    var i16notEqual = i16.notEqual;

    var globImporti16 = i16check(imports.g1);   
  
    var b16 = stdlib.SIMD.Bool8x16;

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 12, 12);

    var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
    var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    var loopCOUNT = 5;

    function testLessThan() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            mask = i16lessThan(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testLessThanOrEqual() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i16lessThanOrEqual(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testGreaterThan() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i16greaterThan(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testGreaterThanOrEqual() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i16greaterThanOrEqual(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testEqual() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i16equal(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testNotEqual() {
        var b = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i16notEqual(b, c);
            result = i16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    return { testLessThan: testLessThan, testLessThanOrEqual: testLessThanOrEqual, testGreaterThan: testGreaterThan, testGreaterThanOrEqual: testGreaterThanOrEqual, testEqual: testEqual, testNotEqual: testNotEqual };
}

var m = asmModule(this, { g1: SIMD.Int8x16(100, -1073741824, -1028, -102, 127, -38, -92929, -128, 52, 127, -127, -129, 129, 0, 88, 100234) });

equalSimd([-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1], m.testLessThan(), SIMD.Int8x16, "Func1");
equalSimd([-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1], m.testLessThanOrEqual(), SIMD.Int8x16, "Func2");
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m.testGreaterThan(), SIMD.Int8x16, "Func3");
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m.testGreaterThanOrEqual(), SIMD.Int8x16, "Func4");
equalSimd([-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1], m.testEqual(), SIMD.Int8x16, "Func5");
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m.testNotEqual(), SIMD.Int8x16, "Func6");

print("PASS");



