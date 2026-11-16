//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;


    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    var f4splat = f4.splat;
    
    var f4fromInt32x4 = f4.fromInt32x4;
    var f4fromInt32x4Bits = f4.fromInt32x4Bits;
    var f4abs = f4.abs;
    var f4neg = f4.neg;
    var f4add = f4.add;
    var f4sub = f4.sub;
    var f4mul = f4.mul;
    var f4div = f4.div;
    var f4min = f4.min;
    var f4max = f4.max;
    var f4sqrt = f4.sqrt;
    
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;


    var f4extractLane = f4.extractLane;

    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    
    var gval = 1234;
    var gval2 = 1234.0;


    var loopCOUNT = 3;

    function func1(a, i, b, count)
    {
        a = f4check(a);
        i = i | 0;
        b = f4check(b);
        count = count | 0;

        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (count|0)) {

            x = f4add(a, b);

            loopIndex = (loopIndex + i) | 0;
        }

        return f4check(x);
    }
    
    function func2(a, count, b, c, d, i)
    {
        a = f4check(a);
        count = count | 0;
        b = f4check(b);
        c = f4check(c);
        d = f4check(d);
        i = i | 0;
        var x = f4(0.0,0.0,0.0,0.0);
        var y = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (count | 0) ; loopIndex = (loopIndex + i) | 0)
        {
            x = f4check(func1(a, 1, b, 3));
            y = f4check(func1(c, 1, d, 3));
        }

        return f4check(f4add(x,y));
    }

    function func3(a, float1, b, c, float2, d, i, e, f, double3, g, h,  float4)
    {
        a = f4check(a);
        float1 = fround(float1);
        b = f4check(b);
        c = f4check(c);
        float2 = fround(float2);
        d = f4check(d);
        i = i | 0;
        e = f4check(e);
        f = f4check(f);
        double3 = +double3;
        g = f4check(g);
        h = f4check(h);
        float4 = fround(float4);        
        
        var x = f4(0.0,0.0,0.0,0.0);
        var y = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + i) | 0)
        {
            x = f4check(func2(a, 3, b, c, d, 1));
            y = f4check(func2(e, 3, f, g, h, 1));
        }

        return f4check(f4add(x,y));
    }

    function func4() {
        var value1 = f4(0.0, 0.0, 0.0, 0.0);
        var i = 0;

        for (i = 0; (i | 0) < 1000; i = (i + 1)|0) {
            value1 = f4add(value1, f4splat(1.0));
            if ((i | 0) == 300) {
                return f4check(value1);
            }
        }
    }
    
    // TODO: Test conversion of returned value
    function value()
    {
        var ret = 1.0;
        var i = 1.0;


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            ret = ret + i;

            loopIndex = (loopIndex + 1) | 0;
        }

        return +ret;
    }

    function fctest(a)
    {
        a = i4check(a);
        return a;
    }
    function fcBug_1()
    {
        var x = f4(-1.0, -2.0, -3.0, -4.0);
        var k = i4(-1, -2, -3, -4);
        k = i4check(fctest(k));
        return f4check(x);
    }
    function fcBug_2()
    {
        var x = f4(-1.0, -2.0, -3.0, -4.0);
        var k = i4(-1, -2, -3, -4);
        x = f4check(fcBug_1());
        return i4check(k);
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret = 0.0;
        var ret1 = fround(0);
        var a = f4(1.0, 2.0, 3.0, 4.0);
        ret = +f4extractLane(a, 0);
        ret1 = fround(f4extractLane(a, 0));
    }

    return {func1:func1, func2:func2, func3:func3, func4:func4, fcBug1:fcBug_1, fcBug2:fcBug_2};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

var s1 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s2 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s3 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s4 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s5 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s6 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s7 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
var s8 = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);

var c;
c = m.func1(s1, 1, s2, 3);
equalSimd([2.000000,4.000000,6.000000,8.000000], c, SIMD.Float32x4, "func1");

c = m.func2(s1, 3, s2, s3, s4, 1);
equalSimd([4.000000,8.000000,12.000000,16.000000], c, SIMD.Float32x4, "func1");

c = m.func3(s1, 33.2, s2, s3, 35.1, s4, 1, s5, s6, 1.0, s7, s8, 5.3);
equalSimd([8.000000,16.000000,24.000000,32.000000], c, SIMD.Float32x4, "func1");

c = m.func4();
equalSimd([301.000000,301.000000,301.000000,301.000000], c, SIMD.Float32x4, "func1");

c = m.fcBug1();
equalSimd([-1, -2, -3, -4], c, SIMD.Float32x4, 'fcBug1');

c = m.fcBug2();
equalSimd([-1, -2, -3, -4], c, SIMD.Int32x4, 'fcBug2');

WScript.Echo("PASS");