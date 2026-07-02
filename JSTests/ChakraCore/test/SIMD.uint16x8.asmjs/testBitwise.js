//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8splat = ui8.splat;

    var ui8add = ui8.add;
    var ui8sub = ui8.sub;
    var ui8mul = ui8.mul;
    var ui8and = ui8.and;
    var ui8or = ui8.or;
    var ui8xor = ui8.xor;
    var ui8not = ui8.not;
    var ui8neg = ui8.neg;

    var globImportui8 = ui8check(imports.g1);       // global var import

    var ui8g1 = ui8(1065353216, 1073741824, 1077936128, 1082130432, 1040103192, 1234123832, 0807627311, 0659275670);          // global var initialized
    var ui8g2 = ui8(353216, 492529, 1128, 1085, 3692, 3937, 9755, 2638);          // global var initialized

    var gval = 1234;
    var gval2 = 1234.0;
    
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;

    var loopCOUNT = 3;

    function func1()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = ui8and(b, b);
            b = ui8or(c, c);
            d = ui8xor(b, d);
            d = ui8not(d);
            d = ui8neg(d);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(d));
    }
    
    function func2()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            d = ui8and(ui8g1, ui8g2);
            d = ui8or(d, b);
            d = ui8xor(d, globImportui8);
            d = ui8not(d);
            d = ui8neg(d);
            
        }

        return i8check(i8fu8(d));
    }

    function func3()
    {
        var b = ui8(5033, 3401, 665, 32234, 948, 2834, 7748, 25);
        var c = ui8(34183, 212344, 569437, 65534, 87654, 984, 3434, 9876);
        var d = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            ui8g1 = ui8and(ui8g1, ui8g2);
            ui8g1 = ui8or(ui8g1, b);
            ui8g1 = ui8xor(ui8g1, c);
            ui8g1 = ui8not(ui8g1);
            ui8g1 = ui8neg(ui8g1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i8check(i8fu8(ui8g1));
    }
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, {g1:SIMD.Uint16x8(1065353216, 1073741824, 1077936128, 1082130432, 383829393, 39283838, 92929, 109483922)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.func1());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.func2());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.func3());

equalSimd([1, 1, 1, 1, 1, 1, 1, 1], ret1, SIMD.Uint16x8, "Func1");
equalSimd([5034, 3402, 666, 32235, 49710, 25421, 21839, 40334], ret2, SIMD.Uint16x8, "Func2");
equalSimd([38447, 12338, 45765, 33301, 20955, 3307, 13096, 11408], ret3, SIMD.Uint16x8, "Func3");
print("PASS");
