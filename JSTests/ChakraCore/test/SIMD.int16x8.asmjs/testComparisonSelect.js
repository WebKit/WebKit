//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8lessThan = i8.lessThan;
    var i8equal = i8.equal;
    var i8greaterThan = i8.greaterThan;
    var i8lessThanOrEqual = i8.lessThanOrEqual;
    var i8greaterThanOrEqual = i8.greaterThanOrEqual;
    var i8notEqual = i8.notEqual;
    var i8select = i8.select;

    var b8 = stdlib.SIMD.Bool16x8;

    var globImporti8 = i8check(imports.g1);       // global var import
  
    var i8g1 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1040103192, 1234123832, 0807627311, 0659275670);          // global var initialized
    var i8g2 = i8(353216, -492529, -1128, 1085, 3692, 3937, 9755, 2638);          // global var initialized

    var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
    var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
    var d = i8(0, 0, 0, 0, 0, 0, 0, 0);

    var loopCOUNT = 5;

    function testLessThan() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            mask = i8lessThan(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    function testLessThanOrEqual() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i8lessThanOrEqual(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    function testGreaterThan() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i8greaterThan(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    function testGreaterThanOrEqual() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i8greaterThanOrEqual(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    function testEqual() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i8equal(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    function testNotEqual() {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = i8notEqual(b, c);
            d = i8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }

    return { testLessThan: testLessThan, testLessThanOrEqual: testLessThanOrEqual, testGreaterThan: testGreaterThan, testGreaterThanOrEqual: testGreaterThanOrEqual, testEqual: testEqual, testNotEqual: testNotEqual };
}

var m = asmModule(this, { g1: SIMD.Int16x8(106533216, 1073741824, 1077936128, 1082130432, 383829393, 39283838, 92929, 109483922) });

equalSimd([5033, -3401, 665, -32234, -948, -984, 3434, -9876], m.testLessThan(), SIMD.Int16x8, "Func1");
equalSimd([5033, -3401, 665, -32234, -948, -984, 3434, -9876], m.testLessThanOrEqual(), SIMD.Int16x8, "Func2");
equalSimd([31353, 15736, 20387, -2, 22118, 2834, 7748, -25], m.testGreaterThan(), SIMD.Int16x8, "Func3");
equalSimd([31353, 15736, 20387, -2, 22118, 2834, 7748, -25], m.testGreaterThanOrEqual(), SIMD.Int16x8, "Func4");
equalSimd([31353, 15736, 20387, -2, 22118, -984, 3434, -9876], m.testEqual(), SIMD.Int16x8, "Func5");
equalSimd([5033, -3401, 665, -32234, -948, 2834, 7748, -25], m.testNotEqual(), SIMD.Int16x8, "Func6");

print("PASS");



