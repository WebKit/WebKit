//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8lessThan = ui8.lessThan;
    var ui8equal = ui8.equal;
    var ui8greaterThan = ui8.greaterThan;
    var ui8lessThanOrEqual = ui8.lessThanOrEqual;
    var ui8greaterThanOrEqual = ui8.greaterThanOrEqual;
    var ui8notEqual = ui8.notEqual;
    var ui8select = ui8.select;

    var b8 = stdlib.SIMD.Bool16x8;

    var globImportui8 = ui8check(imports.g1);       // global var import
  
    var ui8g1 = ui8(65000, 53888, 36128, 30432, 3192, 3832, 7311, 670);          // global var initialized
    var ui8g2 = ui8(353216, 492529, 1128, 1085, 3692, 3937, 9755, 2638);          // global var initialized

    var loopCOUNT = 5;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;
    function testLessThan()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            mask = ui8lessThan(b, c);
            d = ui8select(mask, b, c);
           
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    function testLessThanOrEqual()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui8lessThanOrEqual(b, c);
            d = ui8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    function testGreaterThan()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui8greaterThan(b, c);
            d = ui8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    function testGreaterThanOrEqual()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui8greaterThanOrEqual(b, c);
            d = ui8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    function testEqual()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui8equal(b, c);
            d = ui8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    function testNotEqual()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var mask = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {

            mask = ui8notEqual(b, c);
            d = ui8select(mask, b, c);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }

    return { testLessThan: testLessThan, testLessThanOrEqual: testLessThanOrEqual, testGreaterThan: testGreaterThan, testGreaterThanOrEqual: testGreaterThanOrEqual, testEqual: testEqual, testNotEqual: testNotEqual };
}

var m = asmModule(this, { g1: SIMD.Uint16x8(106533216, 1073741824, 1077936128, 1082130432, 383829393, 39283838, 92929, 109483922)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits( m.testLessThan());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits( m.testLessThanOrEqual());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits( m.testGreaterThan());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits( m.testGreaterThanOrEqual());
var ret5 = SIMD.Uint16x8.fromInt16x8Bits( m.testEqual());
var ret6 = SIMD.Uint16x8.fromInt16x8Bits( m.testNotEqual());


equalSimd([5033, 3401, 665, 32234, 948, 984, 3434, 25], ret1, SIMD.Uint16x8, "Func1");
equalSimd([5033, 3401, 665, 32234, 948, 984, 3434, 25], ret2, SIMD.Uint16x8, "Func2");
equalSimd([34183, 15736, 45149, 65534, 22118, 2834, 7748, 9876], ret3, SIMD.Uint16x8, "Func3");
equalSimd([34183, 15736, 45149, 65534, 22118, 2834, 7748, 9876], ret4, SIMD.Uint16x8, "Func4");
equalSimd([34183, 15736, 45149, 65534, 22118, 984, 3434, 9876], ret5, SIMD.Uint16x8, "Func5");
equalSimd([5033, 3401, 665, 32234, 948, 2834, 7748, 25], ret6, SIMD.Uint16x8, "Func6");

print("PASS");



