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
    
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;
    var u16splat = u16.splat;
    
    
    //var u16abs = u16.abs;
    //var u16neg = u16.neg;
    var u16add = u16.add;
    var u16sub = u16.sub;
    var u16mul = u16.mul;

    var u16lessThan = u16.lessThan;
    var u16lessThanOrEqual = u16.lessThanOrEqual;
    var u16equal = u16.equal;
    var u16notEqual = u16.notEqual;
    var u16greaterThan = u16.greaterThan;
    var u16greaterThanOrEqual = u16.greaterThanOrEqual;
    var u16select = u16.select;
    var u16and = u16.and;
    var u16or = u16.or;
    var u16xor = u16.xor;
    var u16not = u16.not;
    //var u16shiftLeftByScalar = u16.shiftLeftByScalar;
    //var u16shiftRightByScalar = u16.shiftRightByScalar;
    //var u16shiftRightArithmeticByScalar = u16.shiftRightArithmeticByScalar;

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
    var globImportI8 = u16check(imports.g2);       // global var import
    var globImportB8 = b16check(imports.g3);       // global var import
    
    var f4g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var f4g2 = f4(1194580.33,-11201.5,63236.93,334.8);          // global var initialized

    var u16g1 = u16(1065353216, -1073741824, -1077936128, 1082130432, 103216, -107324, -1076128, 432, 1065353216, -1073741824, -1077936128, 1082130432, 103216, -107324, -1076128, 432); 
    var u16g2 = u16(353216, -492529, -1128, 1085, 0, -1, -2, 3, 353216, -492529, -1128, 1085, 0, -1, -2, 3);     
    var b16g3 = b16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);     

    var gval = 1234;
    var gval2 = 1234.0;
    
    var loopCOUNT = 3;

    function func1(a)
    {
        a = a|0;
        var b = u16(5033,-3401,665,-32234,5033,-3401,665,-32234, -34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534);
        var c = u16(-34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534, 5033,-3401,665,-32234,5033,-3401,665,-32234);
        var d = b16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            if ((a|0) == 0)
            {
                d = u16lessThan(b, c);
                
            } else if ((a|0) == 1)
            {
                d = u16equal(b, c);
            } else if ((a|0) == 2)
            {
                d = u16greaterThan(b, c);
            } else if ((a|0) == 3)
            {
                d = u16lessThanOrEqual(b, c);
            } else if ((a|0) == 4)
            {
                d = u16greaterThanOrEqual(b, c);
            } else if ((a|0) == 5)
            {
                d = u16notEqual(b, c);
            }
            
            globImportB8 = u16notEqual(b, c);
            b16g3 = u16equal(b,c);
            loopIndex = (loopIndex + 1) | 0;
        }

        return b16check(d);
    }
    
    function func2(a)
    {
        a = a|0;
        var b = u16(5033,-3401,665,-32234,5033,-3401,665,-32234, -34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534);
        var c = u16(-34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534, 5033,-3401,665,-32234,5033,-3401,665,-32234);
        var d = b16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            if ((a|0) == 0)
            {
                d = u16lessThan(u16g1, u16g2);
                
            } else if ((a|0) == 1)
            {
                d = u16equal(u16g1, u16g2);
            } else if ((a|0) == 2)
            {
                d = u16greaterThan(u16g1, u16g2);
            }
            else if ((a|0) == 3)
            {
                d = u16lessThanOrEqual(u16g1, u16g2);
            } else if ((a|0) == 4)
            {
                d = u16greaterThanOrEqual(u16g1, u16g2);
            } else if ((a|0) == 5)
            {
                d = u16notEqual(u16g1, u16g2);
            }

        }

        return b16check(d);
    }

    function func3(a)
    {
        a = a|0;
        var b = u16(5033,-3401,665,-32234,5033,-3401,665,-32234, -34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534);
        var c = u16(-34183, 212344, -569437, 65534,-34183, 212344, -569437, 65534, 5033,-3401,665,-32234,5033,-3401,665,-32234);
        var d = b16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var loopIndex = 0;

        loopIndex = loopCOUNT | 0;
        do {

            if ((a|0) == 0)
            {
                globImportB8 = u16lessThan(globImportI8, u16g2);
                
            } else if ((a|0) == 1)
            {
                globImportB8 = u16equal(globImportI8, u16g2);
            } else if ((a|0) == 2)
            {
                globImportB8 = u16greaterThan(globImportI8, u16g2);
            }
            else if ((a|0) == 3)
            {
                globImportB8 = u16lessThanOrEqual(globImportI8, u16g2);
            } else if ((a|0) == 4)
            {
                globImportB8 = u16greaterThanOrEqual(globImportI8, u16g2);
            }else if ((a|0) == 5)
            {
                globImportB8 = u16notEqual(globImportI8, u16g2);
            }

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return b16check(globImportB8);
    }
    
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Uint8x16(216, 824, -0.0, 432, -1065353216, -1073741824,-1077936128, -1082130432, 216, 824, -0.0, 432, -1065353216, -1073741824,-1077936128, -1082130432), g3:SIMD.Bool8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)});


equalSimd([false, false, true, true, false, false, true, true, true, true, false, false, true, true, false, false], m.func1(0), SIMD.Bool8x16, "Func1")
equalSimd([true, true, true, true, false, true, true, false, true, true, true, true, false, true, true, false], m.func2(0), SIMD.Bool8x16, "Func2")
equalSimd([false, false, true, false, false, true, true, true, false, false, true, false, false, true, true, true], m.func3(0), SIMD.Bool8x16, "Func3")
equalSimd([false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false], m.func1(1), SIMD.Bool8x16, "Func1")
equalSimd([false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false], m.func2(1), SIMD.Bool8x16, "Func2")
equalSimd([false, false, false, false, true, false, false, false, false, false, false, false, true, false, false, false], m.func3(1), SIMD.Bool8x16, "Func3")
equalSimd([true, true, false, false, true, true, false, false, false, false, true, true, false, false, true, true], m.func1(2), SIMD.Bool8x16, "Func1")
equalSimd([false, false, false, false, true, false, false, true, false, false, false, false, true, false, false, true], m.func2(2), SIMD.Bool8x16, "Func2")
equalSimd([true, true, false, true, false, false, false, false, true, true, false, true, false, false, false, false], m.func3(2), SIMD.Bool8x16, "Func3")
equalSimd([false, false, true, true, false, false, true, true, true, true, false, false, true, true, false, false], m.func1(3), SIMD.Bool8x16, "Func1")
equalSimd([true, true, true, true, false, true, true, false, true, true, true, true, false, true, true, false], m.func2(3), SIMD.Bool8x16, "Func2")
equalSimd([false, false, true, false, true, true, true, true, false, false, true, false, true, true, true, true], m.func3(3), SIMD.Bool8x16, "Func3")
equalSimd([true, true, false, false, true, true, false, false, false, false, true, true, false, false, true, true], m.func1(4), SIMD.Bool8x16, "Func1")
equalSimd([false, false, false, false, true, false, false, true, false, false, false, false, true, false, false, true], m.func2(4), SIMD.Bool8x16, "Func2")
equalSimd([true, true, false, true, true, false, false, false, true, true, false, true, true, false, false, false], m.func3(4), SIMD.Bool8x16, "Func3")
equalSimd([true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], m.func1(5), SIMD.Bool8x16, "Func1")
equalSimd([true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], m.func2(5), SIMD.Bool8x16, "Func2")
equalSimd([true, true, true, true, false, true, true, true, true, true, true, true, false, true, true, true], m.func3(5), SIMD.Bool8x16, "Func3")

/*
printSimdBaseline(m.func1(0), "SIMD.Bool8x16", "m.func1(0)", "Func1");
printSimdBaseline(m.func2(0), "SIMD.Bool8x16", "m.func2(0)", "Func2");
printSimdBaseline(m.func3(0), "SIMD.Bool8x16", "m.func3(0)", "Func3");

printSimdBaseline(m.func1(1), "SIMD.Bool8x16", "m.func1(1)", "Func1");
printSimdBaseline(m.func2(1), "SIMD.Bool8x16", "m.func2(1)", "Func2");
printSimdBaseline(m.func3(1), "SIMD.Bool8x16", "m.func3(1)", "Func3");

printSimdBaseline(m.func1(2), "SIMD.Bool8x16", "m.func1(2)", "Func1");
printSimdBaseline(m.func2(2), "SIMD.Bool8x16", "m.func2(2)", "Func2");
printSimdBaseline(m.func3(2), "SIMD.Bool8x16", "m.func3(2)", "Func3");

printSimdBaseline(m.func1(3), "SIMD.Bool8x16", "m.func1(3)", "Func1");
printSimdBaseline(m.func2(3), "SIMD.Bool8x16", "m.func2(3)", "Func2");
printSimdBaseline(m.func3(3), "SIMD.Bool8x16", "m.func3(3)", "Func3");

printSimdBaseline(m.func1(4), "SIMD.Bool8x16", "m.func1(4)", "Func1");
printSimdBaseline(m.func2(4), "SIMD.Bool8x16", "m.func2(4)", "Func2");
printSimdBaseline(m.func3(4), "SIMD.Bool8x16", "m.func3(4)", "Func3");

printSimdBaseline(m.func1(5), "SIMD.Bool8x16", "m.func1(5)", "Func1");
printSimdBaseline(m.func2(5), "SIMD.Bool8x16", "m.func2(5)", "Func2");
printSimdBaseline(m.func3(5), "SIMD.Bool8x16", "m.func3(5)", "Func3");
*/
print("PASS");



