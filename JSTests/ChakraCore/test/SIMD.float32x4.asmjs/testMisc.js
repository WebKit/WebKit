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

    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
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

    var f4reciprocalApproximation = f4.reciprocalApproximation;
    var f4reciprocalSqrtApproximation = f4.reciprocalSqrtApproximation;

    var f4sqrt = f4.sqrt;

    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;

    
    
    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import

    var f4g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var f4g2 = f4(1194580.33,-11201.5,63236.93,334.8);          // global var initialized

    var i4g1 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var i4g2 = i4(353216, -492529, -1128, 1085);          // global var initialized

    var gval = 1234;
    var gval2 = 1234.0;


    
    var loopCOUNT = 3;

    function func1()
    {
        var b = f4(5033.2,-3401.0,665.34,-32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = f4(0.0, 0.0, 0.0, 0.0);
        var loopIndex = 0;

        loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = f4min(b, c);
            b = f4max(b, c);
            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(f4reciprocalApproximation(b));
    }
    
    function func2()
    {
        var b = f4(5033.2,-3401.0,665.34,-32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = f4(0.0, 0.0, 0.0, 0.0);
        var loopIndex = 0;

        loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = f4min(b, c);
            b = f4max(b, c);
            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(f4reciprocalSqrtApproximation(b));
    }

    function bug1() //simd tmp reg reuse.
    {
        var a = i4(1,2,3,4);
        var x = f4(-1.0, -2.0, -3.0, -4.0);
        f4add(x,x);
        return i4check(a);
    }

    return {func1:func1, func2:func2, bug1:bug1};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

var c;
c = m.func1();
equalSimd([0.00019868074741680175, 0.00000470933673568652, 0.0015029908390715718, 0.00001525902553112246], c, SIMD.Float32x4, "func1")

c = m.func2();
equalSimd([0.01409541629254818, 0.002170100575312972, 0.038768425583839416, 0.003906280267983675], c, SIMD.Float32x4, "func2")

c = m.bug1();
equalSimd([1,2,3,4], c, SIMD.Int32x4, "bug1")


print("PASS");