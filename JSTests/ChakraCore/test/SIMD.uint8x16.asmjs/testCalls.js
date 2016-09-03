//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    /*
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    var i4fromFloat64x2 = i4.fromFloat64x2;
    var i4fromFloat64x2Bits = i4.fromFloat64x2Bits;
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
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;

    var u16extractLane = u16.extractLane;

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
    var globImportU16 = u16check(imports.g2);       // global var import
    
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = u16(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432, 1033216, -11824, -106128, 1081302, 53216, -10, -10779, 1082130432);          // global var initialized
    
    var gval = 1234;
    var gval2 = 1234.0;


    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;
    var u16fi16 = u16.fromInt8x16Bits;
    var loopCOUNT = 3;

    function func1(a, b)
    {
        a = i16check(a);
        b = i16check(b);
        var x = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, -1028, -1029, -1030, -1031);;

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = g2;

            loopIndex = (loopIndex + 1) | 0;
        }
        x = globImportU16;
        g2 = x;
        return i16check(i16fu16(x));
    }
    
    function func2(a, b, c, d)
    {
        a = i16check(a);
        b = i16check(b);
        c = i16check(c);
        d = i16check(d);
        var x = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, -1028, -1029, -1030, -1031);
        var y = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = u16fi16(i16check(func1(a, b)));
            y = u16fi16(i16check(func1(c, d)));
        }

        //return i16check(u16add(x,y));
        return i16check(i16fu16(x));
    }

    function func3(a, b, c, d, e, f, g, h)
    {
        a = i16check(a);
        b = i16check(b);
        c = i16check(c);
        d = i16check(d);
        e = i16check(e);
        f = i16check(f);
        g = i16check(g);
        h = i16check(h);        
        
        var x = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, -1028, -1029, -1030, -1031);
        var y = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = u16fi16(i16check(func2(a, b, c, d)));
            y = u16fi16(i16check(func2(e, f, g, h)));
            
        }

        //return i16check(u16add(x,y));
        return i16check(i16fu16(x));
    }

    function func4() { //Testing for a bug while returning SIMD values from a loop
        var value1 = u16(-1, -2, -3, -4, -5, -6, -7, -8, 1024, 1025, 1026, 1027, -1028, -1029, -1030, -1031);
        var i = 0;

        for (i = 0; (i | 0) < 1000; i = (i + 1)|0) {
            //value1 = u16add(value1, u16splat(1));
            if ((i | 0) == 300) {
                return i16check(i16fu16(value1));
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
        a = i16check(a);
        return a;
    }
    function fcBug_1()
    {
        var x = f4(-1.0, -2.0, -3.0, -4.0);
        var k = u16(1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
        k = u16fi16(i16check(fctest(i16fu16(k))));
        return f4check(x);
    }
    function fcBug_2()
    {
        var x = f4(-1.0, -2.0, -3.0, -4.0);
        var k = u16(1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
        x = f4check(fcBug_1());
        return i16check(i16fu16(k));
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret = 0.0;
        var ret1 = 0;
        var a = u16(1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
        ret = +u16extractLane(a, 0);
        ret1 = (u16extractLane(a, 0))|0;
    }

    return {func1:func1, func2:func2, func3:func3, func4:func4, func5:fcBug_1, func6:fcBug_2};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Uint8x16(-106535326, -73741824,-10776128, -10130432, -10653536, -10741824,-1077928, -12130432, 106535326, 73741824,10776128, 10130432, 10653536, 10741824,1077928, 12130432)});

//print(SIMD.Uint8x16(-1065353213, -1073741824,-1077936128, -1082130432, -1065353216, -1073741824,-1077936128, -1082130432).toString());

var s1 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s2 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s3 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s4 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s5 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s6 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s7 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);
var s8 = SIMD.Uint8x16(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0);

var i16fu16 = SIMD.Int8x16.fromUint8x16Bits;
var u16fi16 = SIMD.Uint8x16.fromInt8x16Bits;

var ret1 = u16fi16(m.func1(i16fu16(s1), i16fu16(s2)));
var ret2 = u16fi16(m.func2(i16fu16(s1), i16fu16(s2), i16fu16(s3), i16fu16(s4)));
var ret3 = u16fi16(m.func3(i16fu16(s1), i16fu16(s2), i16fu16(s3), i16fu16(s4), i16fu16(s5), i16fu16(s6), i16fu16(s7), i16fu16(s8)));
var ret4 = u16fi16(m.func4());
var ret5 = m.func5();
var ret6 = u16fi16(m.func6());


equalSimd([98, 0, 192, 0, 160, 192, 88, 128, 158, 0, 64, 0, 96, 64, 168, 128], ret1, SIMD.Uint8x16, "func1")
equalSimd([98, 0, 192, 0, 160, 192, 88, 128, 158, 0, 64, 0, 96, 64, 168, 128], ret2, SIMD.Uint8x16, "func2")
equalSimd([98, 0, 192, 0, 160, 192, 88, 128, 158, 0, 64, 0, 96, 64, 168, 128], ret3, SIMD.Uint8x16, "func3")
equalSimd([255, 254, 253, 252, 251, 250, 249, 248, 0, 1, 2, 3, 252, 251, 250, 249], ret4, SIMD.Uint8x16, "func4")
equalSimd([-1.0, -2.0, -3.0, -4.0], ret5, SIMD.Float32x4, "func5 - Bug1")
equalSimd([1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8], ret6, SIMD.Uint8x16, "func6 - Bug1")

/*
printSimdBaseline(ret1, "SIMD.Uint8x16", "ret1", "func1");
printSimdBaseline(ret2, "SIMD.Uint8x16", "ret2", "func2");
printSimdBaseline(ret3, "SIMD.Uint8x16", "ret3", "func3");
printSimdBaseline(ret4, "SIMD.Uint8x16", "ret4", "func4");
*/



print("PASS");

