//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16select = ui16.select;
    var ui16lessThan = ui16.lessThan;
    var ui16equal = ui16.equal;
    var ui16greaterThan = ui16.greaterThan;
    var ui16lessThanOrEqual = ui16.lessThanOrEqual;
    var ui16greaterThanOrEqual = ui16.greaterThanOrEqual;
    var ui16notEqual = ui16.notEqual;

    var b16 = stdlib.SIMD.Bool8x16;

    var globImportui16 = ui16check(imports.g1);   
  
    var ui16g1 = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var ui16g2 = ui16(256, 255, 128, 127, 0, 0, 1000, 1000, 5, 15, 3, 399, 299, 21, 45, 22);

    var loopCOUNT = 6;

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;

    function testLessThan() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            mask = ui16lessThan(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    function testLessThanOrEqual() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui16lessThanOrEqual(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    function testGreaterThan() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui16greaterThan(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    function testGreaterThanOrEqual() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui16greaterThanOrEqual(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    function testEqual() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui16equal(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    function testNotEqual() {
        var b = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var c = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var d = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui16notEqual(b, c);
            d = ui16select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(d));
    }

    return { testLessThan: testLessThan, testLessThanOrEqual: testLessThanOrEqual, testGreaterThan: testGreaterThan, testGreaterThanOrEqual: testGreaterThanOrEqual, testEqual: testEqual, testNotEqual: testNotEqual };
}

var m = asmModule(this, { g1: SIMD.Uint8x16(100, -1073741824, -1028, -102, 127, -38, -92929, -128, 52, 127, -127, -129, 129, 0, 88, 100234) });

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.testLessThan());
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.testLessThanOrEqual());
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.testGreaterThan());
var ret4 = SIMD.Uint8x16.fromInt8x16Bits(m.testGreaterThanOrEqual());
var ret5 = SIMD.Uint8x16.fromInt8x16Bits(m.testEqual());
var ret6 = SIMD.Uint8x16.fromInt8x16Bits(m.testNotEqual());

equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1], ret1, SIMD.Uint8x16, "Func1");
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1], ret2, SIMD.Uint8x16, "Func2");
equalSimd([16, 15, 14, 13, 12, 11, 10, 9, 9, 10, 11, 12, 13, 14, 15, 16], ret3, SIMD.Uint8x16, "Func3");
equalSimd([16, 15, 14, 13, 12, 11, 10, 9, 9, 10, 11, 12, 13, 14, 15, 16], ret4, SIMD.Uint8x16, "Func4");
equalSimd([16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1], ret5, SIMD.Uint8x16, "Func5");
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], ret6, SIMD.Uint8x16, "Func6");

print("PASS");



