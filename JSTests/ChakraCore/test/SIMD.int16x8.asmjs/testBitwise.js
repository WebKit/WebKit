//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8splat = i8.splat;
  
    //var i8abs = i8.abs;
    var i8neg = i8.neg;
    var i8add = i8.add;
    var i8sub = i8.sub;
    var i8mul = i8.mul;
    //var i8select = i8.select;
    var i8and = i8.and;
    var i8or = i8.or;
    var i8xor = i8.xor;
    var i8not = i8.not;
  
    var globImporti8 = i8check(imports.g2);       // global var import

    var i8g1 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1040103192, 1234123832, 0807627311, 0659275670);          // global var initialized
    var i8g2 = i8(353216, -492529, -1128, 1085, 3692, 3937, 9755, 2638);          // global var initialized

    var gval = 1234;
    var gval2 = 1234.0;

    var loopCOUNT = 3;

    function func1()
    {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = i8and(b, b);
            b = i8or(c, c);
            d = i8xor(b, d);
            d = i8not(d);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(d);
    }
    
    function func2()
    {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            d = i8and(i8g1, i8g2);
            d = i8or(d, b);
            d = i8xor(d, globImporti8);
            d = i8not(d);
            
        }

        return i8check(d);
    }

    function func3()
    {
        var b = i8(5033, -3401, 665, -32234, -948, 2834, 7748, -25);
        var c = i8(-34183, 212344, -569437, 65534, 87654, -984, 3434, -9876);
        var d = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            i8g1 = i8and(i8g1, i8g2);
            i8g1 = i8or(i8g1, b);
            i8g1 = i8xor(i8g1, c);
            i8g1 = i8not(i8g1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i8check(i8g1);
    }
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, { g1: SIMD.Float32x4(90934.2, 123.9, 419.39, 449.0), g2:SIMD.Int16x8(-1065353216, -1073741824, -1077936128, -1082130432, -383829393, -39283838, -92929, -109483922) /*, g3: SIMD.Float64x2(110.20, 58967.0, 14511.670, 191766.23431)*/ });

equalSimd([-1, -1, -1, -1, -1, -1, -1, -1], m.func1(), SIMD.Int16x8, "Func1");
equalSimd([-5034, 3400, -666, 32233, 15324, 25423, 21839, 26742], m.func2(), SIMD.Int16x8, "Func2");
equalSimd([-27089, 12336, -19771, -32233, 22485, 3301, -13094, -9868], m.func3(), SIMD.Int16x8, "Func3");
print("PASS");
