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

    

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    
    var g1 = f4(0.0,1.0,2.0,3.0);          // global var initialized
    var g2 = i4(0,1,2,3);          // global var initialized
    
    var gval = 1234;


    var loopIndex = 0;
    var loopCOUNT = 3;

    function splat1()
    {
        var x = i4(0,0,0,0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i4splat(100);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(x);
    }
    
    function splat2()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i4splat(value()|0);

        }

        return i4check(x);
    }

    function splat3()
    {
        var x = i4(0,0,0,0);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i4splat(gval);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(x);
    }
    
    function value()
    {
        var ret = 1;
        var i = 0;


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            for (i = 0; (i|0) < 100; i = (i + 1)|0)
                ret = (ret + i)|0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return ret | 0;
    }
    
    return {func1:splat1, func2:splat2, func3:splat3};
}

var m = asmModule(this, {g1:SIMD.Float32x4(9,9,9,9), g2:SIMD.Int32x4(1, 2, 3, 4)});

var ret1 = m.func1(SIMD.Int32x4(1,2,3,4), SIMD.Float32x4(1,2,3,4));
var ret2 = m.func2(SIMD.Int32x4(1,2,3,4), SIMD.Float32x4(1,2,3,4));
var ret3 = m.func3(SIMD.Int32x4(1,2,3,4), SIMD.Float32x4(1,2,3,4));


equalSimd([100, 100, 100, 100], ret1, SIMD.Int32x4, "");
equalSimd([14851, 14851, 14851, 14851], ret2, SIMD.Int32x4, "");
equalSimd([1234, 1234, 1234, 1234], ret3, SIMD.Int32x4, "");
print("PASS");
