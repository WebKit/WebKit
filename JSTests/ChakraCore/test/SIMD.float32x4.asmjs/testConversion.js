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
    
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    var f4splat = f4.splat;
    
    var f4fromInt32x4 = f4.fromInt32x4;
    var f4fromUint32x4 = f4.fromUint32x4;
    var f4fromInt32x4Bits = f4.fromInt32x4Bits;
    
    var f4fromInt16x8Bits = f4.fromInt16x8Bits;
    var f4fromInt8x16Bits = f4.fromInt8x16Bits;
    var f4fromUint32x4Bits = f4.fromUint32x4Bits;
    var f4fromUint16x8Bits = f4.fromUint16x8Bits;
    var f4fromUint8x16Bits = f4.fromUint8x16Bits;

    
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

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    
    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var globImportU4 = u4check(imports.g3);
    
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432); 
    var g3 = u4(3216, -1, 0, -1082130432); 
    
    var g4 = i8(106516, 1073741824,1077936128, 108213032, -1065353216, -1073741824,-1077936128, -1082130432);
    var g5 = u4(106553216, 10737824,77936128, 108132);
    var g6 = u8(1065353216, 1073741824,1077936128, 1082130432, -1065353216, -1073741824,-1077936128, -1082130432);
    var g7 = u16(106535, 1014824,1076128, 108212, -1065353216, -1073724,-77936128, -1082130432, 10653216, 1741824, 7936128, 108432, -103216, -1741824, -1128, -1082130432);
    var g8 = i16(106516, 1073741824,1077936128, 108213032, -1065353216, -1073741824,-1077936128, -1082130432, 106516, 1073741824,1077936128, 108213032, -1065353216, -1073741824,-1077936128, -1082130432);
    var gval = 1234;
    var gval2 = 1234.0;


    var loopIndex = 0;
    var loopCOUNT = 3;


/*    
    function conv2()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = f4fromFloat64x2(globImportD2);

        }

        return f4check(x);
    }

    function conv3()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromFloat64x2(g3);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
    
        return f4check(x);
    }
    
    function conv4()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = d2(21.54, 2.334);
        var loopIndex = 0;
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = f4fromFloat64x2Bits(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(x);
    }
    
    function conv5()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = f4fromFloat64x2Bits(globImportD2);

        }

        return f4check(x);
    }

    function conv6()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromFloat64x2Bits(g3);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(x);
    }
*/    
    ////
    
    function conv7()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = i4(1034, 22342,1233, 40443);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = f4fromInt32x4(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(x);
    }
    
    function conv8()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = f4fromInt32x4(globImportI4);

        }

        return f4check(x);
    }

    function conv9()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromInt32x4(g2);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(x);
    }
    
    function conv10()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = i4(1065353216, 1073741824,1077936128, 1082130432);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = f4fromInt32x4Bits(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(x);
    }
    
    function conv11()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = f4fromInt32x4Bits(globImportI4);

        }

        return f4check(x);
    }

    function conv12()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromInt32x4Bits(g2);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(x);
    }

    
    function conv13()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = f4(0.0,0.0,0.0,0.0);
        var z = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromInt16x8Bits(g4);
            y = f4add(x, f4fromUint32x4Bits(g5));
            y = f4add(y ,f4fromUint16x8Bits(g6));
            y = f4add(y, f4fromUint8x16Bits(g7));
             
            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(y);
    }
    function conv14()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = f4(0.0,0.0,0.0,0.0);
        var z = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromInt8x16Bits(g8);
            y = f4add(x, f4fromUint32x4Bits(g5));
            y = f4add(y ,f4fromUint16x8Bits(g6));
            y = f4add(y, f4fromUint8x16Bits(g7));
             
            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(y);
    }
    
    function conv15()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var y = u4(1034, -22342,1233, -1);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = f4fromUint32x4(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(x);
    }
    
    function conv16()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = f4fromUint32x4(globImportU4);

        }

        return f4check(x);
    }

    function conv17()
    {
        var x = f4(0.0,0.0,0.0,0.0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = f4fromUint32x4(g3);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(x);
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
    
    return {/*func1:conv1, func2:conv2, func3:conv3, func4:conv4, func5:conv5, func6:conv6,*/ func7:conv7, func8:conv8, func9:conv9, func10:conv10, func11:conv11, func12:conv12, func13: conv13, func14:conv14, func15:conv15, func16:conv16, func17:conv17};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432), g3:SIMD.Uint32x4(-1065353216, 1073741824, 0, -1082)});

var c;
/*
c = m.func1();
equalSimd([5033.200195,3401.000000,665.340027,32234.099609], c, SIMD.Float32x4, "func1");

c = m.func2();
equalSimd([110.199997,58967.000000,0.000000,0.000000], c, SIMD.Float32x4, "func2");

c = m.func3();
equalSimd([0.123440,1.657800,0.000000,0.000000], c, SIMD.Float32x4, "func3");

c = m.func4();
equalSimd([405648183006089761369226215424.000000,2.836562,0.000000,2.041750], c, SIMD.Float32x4, "func4");

c = m.func5();
equalSimd([-107374184.000000,3.430469,0.000000,7.399765], c, SIMD.Float32x4, "func5");

c = m.func6();
equalSimd([-0.000000,1.496880,8026220.000000,1.957225], c, SIMD.Float32x4, "func6");
*/
c = m.func7();
equalSimd([1034.000000,22342.000000,1233.000000,40443.000000], c, SIMD.Float32x4, "func7");

c = m.func8();
equalSimd([-1065353216.000000,-1073741824.000000,-1077936128.000000,-1082130432.000000], c, SIMD.Float32x4, "func8");

c = m.func9();
equalSimd([1065353216.000000,1073741824.000000,1077936128.000000,1082130432.000000], c, SIMD.Float32x4, "func9");

c = m.func10();
equalSimd([1.0,2.0,3.0,4.0], c, SIMD.Float32x4, "func10");

c = m.func11();
equalSimd([-4.0,-2.0,-1.5,-1.0], c, SIMD.Float32x4, "func11");

c = m.func12();
equalSimd([1.0,2.0,3.0,4.0], c, SIMD.Float32x4, "func12");

c = m.func13();
equalSimd([-2.9831537062818825e-7, 3.91155481338501e-8, -5.048728450860812e-29, 1.4110812091639615e-38], c, SIMD.Float32x4, "func13");

c = m.func14();
equalSimd([-2.9831537062818825e-7, 1.5117207833136126e-38, 7.105444298259947e-15, 1.4110812091639615e-38], c, SIMD.Float32x4, "func14");

c = m.func15();
equalSimd([1034, 4294945024, 1233, 4294967296], c, SIMD.Float32x4, "func15");

c = m.func16();
equalSimd([3229614080, 1073741824, 0, 4294966272], c, SIMD.Float32x4, "func16");

c = m.func17();
equalSimd([3216, 4294967296, 0, 3212836864], c, SIMD.Float32x4, "func17");
WScript.Echo("PASS");