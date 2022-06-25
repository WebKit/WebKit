//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i4 = stdlib.SIMD.Int32x4;
    var i4extractLane = i4.extractLane;
    var i4replaceLane = i4.replaceLane;
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

    var f4g1 = f4(-5033.2, -3401.0, 665.34, 32234.1);          // global var initialized
    var f4g2 = f4(1194580.33, -11201.5, 63236.93, 334.8);          // global var initialized

    var i4g1 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var i4g2 = i4(353216, -492529, -1128, 1085);          // global var initialized

    

    var gval = 1234;
    var gval2 = 1234.0;


    var si = i4(20, -1, 10, 2000);
    var loopCOUNT = 3;

    function func5(a)
    {
        a = a|0;
        var b = i4(5033,-3401,665,-32234);
        var c = i4(-34183, 212344, -569437, 65534);
        var d = i4(0, 0, 0, 0);
        var loopIndex = 0;

        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            b = i4(i4extractLane(i4g1, 3) * 2, i4extractLane(i4g2, 2) << 4 | 0, -i4extractLane(i4g1, 1) | 0, i4extractLane(i4g1, 0) | 0);
        }
        return i4check(b);
    }

    
    function func7() {

        var a = 0;
        var b = 0;
        var c = 0;
        var d = 0;
        var e = 0;

        a = i4extractLane(si, 3);   //Todo negative cases. must fail.
        b = i4extractLane(si, 2);
        c = i4extractLane(si, 1);
        d = i4extractLane(si, 0);
        si = i4(a, b, c, d);
        si = i4replaceLane(si, 0, 33);
        si = i4replaceLane(si, 1, b);
        si = i4replaceLane(si, 2, c);
        si = i4replaceLane(si, 3, d);

        return i4check(si);
    }

    return { func5:func5, func7: func7 };
}

var m = asmModule(this, { g1: SIMD.Float32x4(90934.2, 123.9, 419.39, 449.0), g2: SIMD.Int32x4(-1065353216, -1073741824, -1077936128, -1082130432) });

var ret;

ret = m.func5();
equalSimd([-2130706432, -18048, 1073741824, 1065353216],ret, SIMD.Int32x4, "Func5");

ret = m.func7();
equalSimd([33, 10, -1, 20], ret, SIMD.Int32x4, "Func7");
print("PASS");
