//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    //var i4fromFloat64x2 = i4.fromFloat64x2;
    //var i4fromFloat64x2Bits = i4.fromFloat64x2Bits;
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    var i4fromInt16x8Bits   = i4.fromInt16x8Bits  ;
    var i4fromInt8x16Bits   = i4.fromInt8x16Bits  ;
    var i4fromUint32x4Bits  = i4.fromUint32x4Bits ;
    var i4fromUint16x8Bits  = i4.fromUint16x8Bits ;
    var i4fromUint8x16Bits  = i4.fromUint8x16Bits ;
    
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
    //var f4fromFloat64x2 = f4.fromFloat64x2;
    //var f4fromFloat64x2Bits = f4.fromFloat64x2Bits;
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

    /*var d2 = stdlib.SIMD.Float64x2;  
    var d2check = d2.check;
    var d2splat = d2.splat;
    var d2fromFloat32x4 = d2.fromFloat32x4;
    var d2fromFloat32x4Bits = d2.fromFloat32x4Bits;
    var d2fromInt32x4 = d2.fromInt32x4;
    var d2fromInt32x4Bits = d2.fromInt32x4Bits;
    var d2abs = d2.abs;
    var d2neg = d2.neg;
    var d2add = d2.add;
    var d2sub = d2.sub;
    var d2mul = d2.mul;
    var d2div = d2.div;
    var d2clamp = d2.clamp;
    var d2min = d2.min;
    var d2max = d2.max;


    var d2sqrt = d2.sqrt;
    //var d2swizzle = d2.swizzle;
    //var d2shuffle = d2.shuffle;
    var d2lessThan = d2.lessThan;
    var d2lessThanOrEqual = d2.lessThanOrEqual;
    var d2equal = d2.equal;
    var d2notEqual = d2.notEqual;
    var d2greaterThan = d2.greaterThan;
    var d2greaterThanOrEqual = d2.greaterThanOrEqual;
    var d2select = d2.select;
*/
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;
    
    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    //var globImportD2 = d2check(imports.g3);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized
    //var g3 = d2(0.12344,1.6578);          // global var initialized
    
    var g4 = i8(106516, 1073741824,1077936128, 108213032, -1065353216, -1073741824,-1077936128, -1082130432);
    var g5 = u4(106553216, 10737824,77936128, 108132);
    var g6 = u8(1065353216, 1073741824,1077936128, 1082130432, -1065353216, -1073741824,-1077936128, -1082130432);
    var g7 = u16(106535, 1014824,1076128, 108212, -1065353216, -1073724,-77936128, -1082130432, 10653216, 1741824, 7936128, 108432, -103216, -1741824, -1128, -1082130432);
    var g8 = i16(106535, 1014824,1076128, 108212, -1065353216, -1073724,-77936128, -1082130432, 10653216, 1741824, 7936128, 108432, -103216, -1741824, -1128, -1082130432);
    
    var gval = 1234;
    var gval2 = 1234.0;

    var loopCOUNT = 3;
/*
    function conv1()
    {
        var x = i4(0,0,0,0);
        var y = d2(21.54, 2.334);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4fromFloat64x2(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }

    function conv2()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4fromFloat64x2(globImportD2);

        }

        return i4check(x);
    }

    function conv3()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i4fromFloat64x2(g3);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(x);
    }

    function conv4()
    {
        var x = i4(0,0,0,0);
        var y = d2(21.54, 2.334);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4fromFloat64x2Bits(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }

    function conv5()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4fromFloat64x2Bits(globImportD2);

        }

        return i4check(x);
    }

    function conv6()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i4fromFloat64x2Bits(g3);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(x);
    }
*/
    function conv7()
    {
        var x = i4(0,0,0,0);
        var y = f4(1034.0, 22342.0,1233.0, 40443.0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4fromFloat32x4(f4(1034.0, 22342.0,1233.0, 40443.0));

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }
    
    function conv8()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4fromFloat32x4(globImportF4);

        }

        return i4check(x);
    }

    function conv9()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i4fromFloat32x4(g1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(x);
    }

    function conv10()
    {
        var x = i4(0,0,0,0);
        var y = f4(1065353216.0, 1073741824.0,1077936128.0, 1082130432.0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4fromFloat32x4Bits(y)

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }

    function conv11()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4fromFloat32x4Bits(globImportF4);

        }

        return i4check(x);
    }

    function conv12()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i4fromFloat32x4Bits(g1);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(x);
    }

    function conv13()
    {
        // all in range
        var a = f4(2147483520.0, -2147483520.0, 2147483520.0, -0.0);
        var b = i4(0, 0, 0, 0);
        var c = i4(0, 0, 0, 0);
        var d = i4(0, 0, 0, 0);
        
        b = i4fromFloat32x4(a);

        a = f4(2147483520.0, 2147483520.0, 2147483520.0, -0.0);
        c = i4fromFloat32x4(a);
    
        a = f4(-2147483520.0, -2147483520.0, -2147483520.0, -0.0);
        d = i4fromFloat32x4(a)
        
        b = i4add(b, c);
        b = i4add(b, d);
        return i4check(b);
    }

    // out of range
    function conv14()
    {

        var a = f4(2147483648.0, -2147483648.0, 2147483648.0, -0.0);
        var b = i4(0, 0, 0, 0);
        
        b = i4fromFloat32x4(a);
        
        return i4check(b);
    }
    
    // out of range
    function conv15()
    {
        var a = f4(2147483648.0, 2147483648.0, 2147483648.0, -0.0);
        var b = i4(0, 0, 0, 0);
        
        b = i4fromFloat32x4(a);
        return i4check(b);
    }
    
    // out of range
    function conv16()
    {
        var a = f4(-2147483904.0, -2147483648.0, -2147483648.0, -0.0);
        var b = i4(0, 0, 0, 0);
        
        b = i4fromFloat32x4(a);
        
        return i4check(b);
    }
    
    function conv17()
    {
        
        var a = i4(0, 0, 0, 0);
        
        a = i4add(i4fromInt16x8Bits (g4), i4fromInt16x8Bits (g4));
        a = i4add(a, i4fromUint32x4Bits (g5));
        a = i4sub(a, i4fromUint16x8Bits (g6));
        a = i4sub(a, i4fromUint8x16Bits (g7))
        a = i4sub(a, i4fromInt8x16Bits (g8))
        return i4check(a);
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

    return {
    /*
    func1:conv1, 
    func2:conv2, 
    func3:conv3, 
    func4:conv4, 
    func5:conv5, 
    func6:conv6, 
    */
    func7:conv7, 
    func8:conv8, 
    func9:conv9, 
    func10:conv10, 
    func11:conv11, 
    func12:conv12, 
    func13:conv13, 
    func14:conv14,
    func15:conv15,
    func16:conv16,
    func17:conv17
    };
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)/*, g3:SIMD.Float64x2(110.20, 58967.0, 14511.670, 191766.23431)*/});
/*
var ret1 = m.func1();
var ret2 = m.func2();
var ret3 = m.func3();


var ret4 = m.func4();
var ret5 = m.func5();
var ret6 = m.func6();
*/

var ret7 = m.func7();
var ret8 = m.func8();
var ret9 = m.func9();


var ret10 = m.func10();
var ret11 = m.func11();
var ret12 = m.func12();

/*
equalSimd([21, 2, 0, 0], ret1, SIMD.Int32x4, "Test Conversion");
equalSimd([110, 58967, 0, 0], ret2, SIMD.Int32x4, "Test Conversion");
equalSimd([0, 1, 0, 0], ret3, SIMD.Int32x4, "Test Conversion");

equalSimd([1889785610, 1077250621, 824633721, 1073916936], ret4, SIMD.Int32x4, "Test Conversion");
equalSimd([-858993459, 1079741644, 0, 1089260256], ret5, SIMD.Int32x4, "Test Conversion");
equalSimd([-1962628256, 1069521347, 1257566424, 1073383001], ret6, SIMD.Int32x4, "Test Conversion");
*/
equalSimd([1034, 22342, 1233, 40443], ret7, SIMD.Int32x4, "Test Conversion");
equalSimd([90934, 123, 419, 449], ret8, SIMD.Int32x4, "Test Conversion");
equalSimd([5033, 3401, 665, 32234], ret9, SIMD.Int32x4, "Test Conversion");

equalSimd([1316880384, 1317011456, 1317044224, 1317076992], ret10, SIMD.Int32x4, "Test Conversion");
equalSimd([1202821914, 1123536077, 1137816044, 1138786304], ret11, SIMD.Int32x4, "Test Conversion");
equalSimd([1167935898, 1163169792, 1143363011, 1190908979], ret12, SIMD.Int32x4, "Test Conversion");

try{m.func13()}catch(e){print("Error13")};

try{m.func14(); print("Error14");}catch(e){};
try{m.func15(); print("Error15");}catch(e){};
try{m.func16(); print("Error16");}catch(e){};


var ret17 = m.func17();
equalSimd([-1659187366, 1727156384, -475712064, -19815228], ret17, SIMD.Int32x4, "func17")

print("PASS");
