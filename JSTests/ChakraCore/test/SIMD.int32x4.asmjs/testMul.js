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
        var b = i4(5033,-3401,665,-32234);
        var c = i4(-34183, 212344, -569437, 65534);
        var d = i4(0, 0, 0, 0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            d = i4mul(c, b);
            b = i4mul(c, c);
            d = i4mul(b, d);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(d);
    }
    
    function func2()
    {
        var b = i4(5033,-3401,665,-32234);
        var c = i4(-34183, 212344, -569437, 65534);
        var d = i4(0, 0, 0, 0);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            d = i4mul(i4g1, i4g2);
            d = i4mul(d, b);
            d = i4mul(d, globImportI4);
            i4g1 = i4mul(b, c);
            globImportI4 = i4mul(c, c);

        }

        return i4check(d);
    }

    function func3()
    {
        var b = i4(5033,-3401,665,-32234);
        var c = i4(-34183, 212344, -569437, 65534);
        var d = i4(0, 0, 0, 0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            i4g1 = i4mul(i4g1, i4g2);
            i4g1 = i4mul(i4g1, b);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i4check(i4g1);
    }

    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

equalSimd([-1115827495, -164134912, -1773145837, 5242848], m.func1(), SIMD.Int32x4, "Func1");
equalSimd([1306834368, 1908105728, -1740529272, -296213152], m.func2(), SIMD.Int32x4, "Func2");
equalSimd([-1571028992, 1735366152, -150797824, -1726173856], m.func3(), SIMD.Int32x4, "Func3");
print("PASS");