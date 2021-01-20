//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    /*
    var i4splat = i4.splat;
    
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    //var i4abs = i4.abs;
    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
    //var i4swizzle = i4.swizzle;
    //var i4shuffle = i4.shuffle;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;
    //var i4shiftLeftByScalar = i4.shiftLeftByScalar;
    //var i4shiftRightByScalar = i4.shiftRightByScalar;
    //var i4shiftRightArithmeticByScalar = i4.shiftRightArithmeticByScalar;
    */
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;

    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    var f4splat = f4.splat;
    var i8extractLane = i8.extractLane;
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
    //var f4swizzle = f4.swizzle;
    //var f4shuffle = f4.shuffle;
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;

    
    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI8 = i8check(imports.g2);       // global var import
    
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    
    var gval = 1234;
    var gval2 = 1234.0;


    
    var loopCOUNT = 3;

    function func1(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);;

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = g2;

            loopIndex = (loopIndex + 1) | 0;
        }
        x = globImportI8;
        g2 = x;

        return i8check(x);
    }
    
    function func2(a, b, c, d)
    {
        a = i8check(a);
        b = i8check(b);
        c = i8check(c);
        d = i8check(d);
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);
        var y = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i8check(func1(a, b));
            y = i8check(func1(c, d));
            

        }

        //return i8check(i8add(x,y));
        return i8check(x);
    }

    function func3(a, b, c, d, e, f, g, h)
    {
        a = i8check(a);
        b = i8check(b);
        c = i8check(c);
        d = i8check(d);
        e = i8check(e);
        f = i8check(f);
        g = i8check(g);
        h = i8check(h);        
        
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);
        var y = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i8check(func2(a, b, c, d));
            y = i8check(func2(e, f, g, h));
            
        }

        //return i8check(i8add(x,y));
        return i8check(x);
    }

    function func4() { //Testing for a bug while returning SIMD values from a loop
        var value1 = i8(-1, -2, -3, -4, -5, -6, -7, -8);
        var i = 0;

        for (i = 0; (i | 0) < 1000; i = (i + 1)|0) {
            //value1 = i8add(value1, i8splat(1));
            if ((i | 0) == 300) {
                return i8check(value1);
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
        a = i8check(a);
        return a;
    }
    function fcBug_1()
    {
        var x = i4(-1, -2, -3, -4);
        var k = i8(1, 2, 3, 4, 5, 6, 7, 8);
        k = i8check(fctest(k));
        return i4check(x);
    }
    function fcBug_2()
    {
        var x = i4(-1, -2, -3, -4);
        var k = i8(1, 2, 3, 4, 5, 6, 7, 8);
        x = i4check(fcBug_1());
        return i8check(k);
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret = 0.0;
        var ret1 = 0;
        var a = i8(1,2,3,4,5,6,7,8);
        ret = +i8extractLane(a, 0);
        ret1 = (i8extractLane(a, 0))|0;
    }

    return {func1:func1, func2:func2, func3:func3, func4:func4, fcBug_2:fcBug_2, fcBug_1:fcBug_1/*, func5:func5, func6:func6*/};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int16x8(-1065353216, -1073741824,-1077936128, -1082130432, -1065353216, -1073741824,-1077936128, -1082130432)});

var s1 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s2 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s3 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s4 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s5 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s6 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s7 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s8 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);

var ret1 = m.func1(s1, s2);
var ret2 = m.func2(s1, s2, s3, s4);
var ret3 = m.func3(s1, s2, s3, s4, s5, s6, s7, s8);
var ret4 = m.func4();
var c1 = m.fcBug_1();
var c2 = m.fcBug_2();


/* printSimdBaseline(ret1, "SIMD.Int16x8", "ret1", "func1");
printSimdBaseline(ret2, "SIMD.Int16x8", "ret2", "func2");
printSimdBaseline(ret3, "SIMD.Int16x8", "ret3", "func3");
printSimdBaseline(ret4, "SIMD.Int16x8", "ret4", "func4"); */

equalSimd([0, 0, 0, 0, 0, 0, 0, 0], ret1, SIMD.Int16x8, "func1")
equalSimd([0, 0, 0, 0, 0, 0, 0, 0], ret2, SIMD.Int16x8, "func2")
equalSimd([0, 0, 0, 0, 0, 0, 0, 0], ret3, SIMD.Int16x8, "func3")
equalSimd([-1, -2, -3, -4, -5, -6, -7, -8], ret4, SIMD.Int16x8, "func4")
equalSimd([-1, -2, -3, -4], c1, SIMD.Int32x4, 'fcBug1');
equalSimd([1, 2, 3, 4, 5, 6, 7, 8], c2, SIMD.Int16x8, 'fcBug2');

print("PASS");

