//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var b4  = stdlib.SIMD.Bool32x4;
    var b8  = stdlib.SIMD.Bool16x8;
    var b16 = stdlib.SIMD.Bool8x16;
    
    var b4check  = b4.check;
    var b8check  = b8.check;
    var b16check = b16.check;
    
    var b4and  = b4.and;
    var b8and  = b8.and;
    var b16and = b16.and;
    
    var b4or = b4.or;
    var b8or = b8.or;
    var b16or= b16.or;
    
    var b4xor = b4.xor;
    var b8xor = b8.xor;
    var b16xor= b16.xor;
    
    var b4not = b4.not;
    var b8not  = b8.not;
    var b16not = b16.not;
    
    var b4allTrue = b4.allTrue;
    var b8allTrue  = b8.allTrue;
    var b16allTrue = b16.allTrue;
    
    var b4anyTrue = b4.anyTrue;
    var b8anyTrue  = b8.anyTrue;
    var b16anyTrue = b16.anyTrue;
    
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    var u4splat = u4.splat;
    
    var u4fromFloat32x4 = u4.fromFloat32x4;
    var u4fromFloat32x4Bits = u4.fromFloat32x4Bits;
    //var u4abs = u4.abs;
    
    var u4add = u4.add;
    var u4sub = u4.sub;
    var u4mul = u4.mul;
    //var u4swizzle = u4.swizzle;
    //var u4shuffle = u4.shuffle;
    var u4lessThan = u4.lessThan;
    var u4lessThanOrEqual = u4.lessThanOrEqual;
    var u4equal = u4.equal;
    var u4notEqual = u4.notEqual;
    var u4greaterThan = u4.greaterThan;
    var u4greaterThanOrEqual = u4.greaterThanOrEqual;
    var u4select = u4.select;
    var u4and = u4.and;
    var u4or = u4.or;
    var u4xor = u4.xor;
    var u4not = u4.not;
    //var u4shiftLeftByScalar = u4.shiftLeftByScalar;
    //var u4shiftRightByScalar = u4.shiftRightByScalar;
    //var u4shiftRightArithmeticByScalar = u4.shiftRightArithmeticByScalar;

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
    var globImportU4 = u4check(imports.g2);       // global var import
    var globImportB4 = b4check(imports.g3);       // global var import
    
    var f4g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var f4g2 = f4(1194580.33,-11201.5,63236.93,334.8);          // global var initialized

    var u4g1 = u4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var u4g2 = u4(353216, -492529, -1128, 1085);          // global var initialized

    

    var gval = 1234;
    var gval2 = 1234.0;


    
    var loopCOUNT = 3;

    function func1(a)
    {
        a = a|0;
        var b = u4(5033,-3401,665,-32234);
        var c = u4(-34183, 212344, -569437, 65534);
        var d = b4(0,0,0,0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            if ((a|0) == 0)
            {
                d = u4lessThan(b, c);
                
            } else if ((a|0) == 1)
            {
                d = u4equal(b, c);
            } else if ((a|0) == 2)
            {
                d = u4greaterThan(b, c);
            } else if ((a|0) == 3)
            {
                d = u4lessThanOrEqual(b, c);
            } else if ((a|0) == 4)
            {
                d = u4greaterThanOrEqual(b, c);
            } else if ((a|0) == 5)
            {
                d = u4notEqual(b, c);
            }
            

            loopIndex = (loopIndex + 1) | 0;
        }

        return b4check(d);
    }
    
    function func2(a)
    {
        a = a|0;
        var b = u4(5033,-3401,665,-32234);
        var c = u4(-34183, 212344, -569437, 65534);
        var d = b4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            if ((a|0) == 0)
            {
                d = u4lessThan(u4g1, u4g2);
                
            } else if ((a|0) == 1)
            {
                d = u4equal(u4g1, u4g2);
            } else if ((a|0) == 2)
            {
                d = u4greaterThan(u4g1, u4g2);
            }
            else if ((a|0) == 3)
            {
                d = u4lessThanOrEqual(u4g1, u4g2);
            } else if ((a|0) == 4)
            {
                d = u4greaterThanOrEqual(u4g1, u4g2);
            } else if ((a|0) == 5)
            {
                d = u4notEqual(u4g1, u4g2);
            }

        }

        return b4check(d);
    }

    function func3(a)
    {
        a = a|0;
        var b = u4(5033,-3401,665,-32234);
        var c = u4(-34183, 212344, -569437, 65534);
        var d = u4(0, 0, 0, 0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            if ((a|0) == 0)
            {
                globImportB4 = u4lessThan(globImportU4, u4g2);
                
            } else if ((a|0) == 1)
            {
                globImportB4 = u4equal(globImportU4, u4g2);
            } else if ((a|0) == 2)
            {
                globImportB4 = u4greaterThan(globImportU4, u4g2);
            }
            else if ((a|0) == 3)
            {
                globImportB4 = u4lessThanOrEqual(globImportU4, u4g2);
            } else if ((a|0) == 4)
            {
                globImportB4 = u4greaterThanOrEqual(globImportU4, u4g2);
            }else if ((a|0) == 5)
            {
                globImportB4 = u4notEqual(globImportU4, u4g2);
            }

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return b4check(globImportB4);
    }
    
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Uint32x4(-1065353216, -1073741824,-1077936128, -1082130432), g3:SIMD.Bool32x4(0,0,0,0)});


equalSimd([true, false, true, false], m.func1(0), SIMD.Bool32x4, "Func1")
equalSimd([false, true, true, false], m.func2(0), SIMD.Bool32x4, "Func2")
equalSimd([false, true, true, false], m.func3(0), SIMD.Bool32x4, "Func3")
equalSimd([false, false, false, false], m.func1(1), SIMD.Bool32x4, "Func1")
equalSimd([false, false, false, false], m.func2(1), SIMD.Bool32x4, "Func2")
equalSimd([false, false, false, false], m.func3(1), SIMD.Bool32x4, "Func3")
equalSimd([false, true, false, true], m.func1(2), SIMD.Bool32x4, "Func1")
equalSimd([true, false, false, true], m.func2(2), SIMD.Bool32x4, "Func2")
equalSimd([true, false, false, true], m.func3(2), SIMD.Bool32x4, "Func3")
equalSimd([true, false, true, false], m.func1(3), SIMD.Bool32x4, "Func1")
equalSimd([false, true, true, false], m.func2(3), SIMD.Bool32x4, "Func2")
equalSimd([false, true, true, false], m.func3(3), SIMD.Bool32x4, "Func3")
equalSimd([false, true, false, true], m.func1(4), SIMD.Bool32x4, "Func1")
equalSimd([true, false, false, true], m.func2(4), SIMD.Bool32x4, "Func2")
equalSimd([true, false, false, true], m.func3(4), SIMD.Bool32x4, "Func3")
equalSimd([true, true, true, true], m.func1(5), SIMD.Bool32x4, "Func1")
equalSimd([true, true, true, true], m.func2(5), SIMD.Bool32x4, "Func2")
equalSimd([true, true, true, true], m.func3(5), SIMD.Bool32x4, "Func3")

/*
printSimdBaseline(m.func1(0), "SIMD.Bool32x4", "m.func1(0)", "Func1");
printSimdBaseline(m.func2(0), "SIMD.Bool32x4", "m.func2(0)", "Func2");
printSimdBaseline(m.func3(0), "SIMD.Bool32x4", "m.func3(0)", "Func3");

printSimdBaseline(m.func1(1), "SIMD.Bool32x4", "m.func1(1)", "Func1");
printSimdBaseline(m.func2(1), "SIMD.Bool32x4", "m.func2(1)", "Func2");
printSimdBaseline(m.func3(1), "SIMD.Bool32x4", "m.func3(1)", "Func3");

printSimdBaseline(m.func1(2), "SIMD.Bool32x4", "m.func1(2)", "Func1");
printSimdBaseline(m.func2(2), "SIMD.Bool32x4", "m.func2(2)", "Func2");
printSimdBaseline(m.func3(2), "SIMD.Bool32x4", "m.func3(2)", "Func3");

printSimdBaseline(m.func1(3), "SIMD.Bool32x4", "m.func1(3)", "Func1");
printSimdBaseline(m.func2(3), "SIMD.Bool32x4", "m.func2(3)", "Func2");
printSimdBaseline(m.func3(3), "SIMD.Bool32x4", "m.func3(3)", "Func3");

printSimdBaseline(m.func1(4), "SIMD.Bool32x4", "m.func1(4)", "Func1");
printSimdBaseline(m.func2(4), "SIMD.Bool32x4", "m.func2(4)", "Func2");
printSimdBaseline(m.func3(4), "SIMD.Bool32x4", "m.func3(4)", "Func3");

printSimdBaseline(m.func1(5), "SIMD.Bool32x4", "m.func1(5)", "Func1");
printSimdBaseline(m.func2(5), "SIMD.Bool32x4", "m.func2(5)", "Func2");
printSimdBaseline(m.func3(5), "SIMD.Bool32x4", "m.func3(5)", "Func3");
*/
print("PASS");



