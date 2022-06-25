//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var ui4 = stdlib.SIMD.Uint32x4;
    var u4check = ui4.check;
    var ui4splat = ui4.splat;
    var ui4fromFloat32x4 = ui4.fromFloat32x4;
    var ui4fromFloat32x4Bits = ui4.fromFloat32x4Bits;
    var ui4fromInt16x8Bits = ui4.fromInt16x8Bits;
    var ui4fromUint16x8Bits = ui4.fromUint16x8Bits;
    var ui4fromInt8x16Bits = ui4.fromInt8x16Bits;
    var ui4fromUint8x16Bits = ui4.fromUint8x16Bits;;
    
    var i4 = stdlib.SIMD.Int32x4;
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;

    var i8 = stdlib.SIMD.Int16x8;
    var ui8 = stdlib.SIMD.Uint16x8;
    var i16 = stdlib.SIMD.Int8x16;
    var ui16 = stdlib.SIMD.Uint8x16;
    
    var fround = stdlib.Math.fround;
    var i4check = i4.check;
    var globImporti168 = u4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized
    var loopCOUNT = 3;


    var i4fu4 = i4.fromUint32x4Bits;
    var u4fi4 = ui4.fromInt32x4Bits;

    function conv1()
    {
        var x = ui4(0,0,0,0);
       
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui4fromFloat32x4Bits(g1);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fu4(x));
    }
    
    function conv2()
    {
        var x = ui4(0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = u4fi4(globImportI4);
        }
        return i4check(i4fu4(x));
    }

    function conv3()
    {
        var x = ui4(0,0,0,0);
        var y = i4(1,2,3,4);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = u4fi4(y);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
       
        return i4check(i4fu4(x));
    }

    function conv4()
    {
        var x = ui4(0,0,0,0);
        var y = ui8(0,0,0,0,0,0,0,0);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui4fromUint16x8Bits(y);

            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i4check(i4fu4(x));
    }

    function conv5()
    {
        var x = ui4(0,0,0,0);
        var m = i16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui4fromInt8x16Bits(m);
        }
        return i4check(i4fu4(x));
    }
    
    function conv6()
    {
        var x = ui4(0,0,0,0);
        var m = ui16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui4fromUint8x16Bits(m);
        }
        return i4check(i4fu4(x));
    }

    function conv7(v)
    {
        v = f4check(v);
        var x = ui4(0,0,0,0);
        

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui4fromFloat32x4(v);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i4check(i4fu4(x));
    }
    
    function conv8()
    {
        var x = ui4(0,0,0,0);
        var m = i8(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui4fromInt16x8Bits(m);
        }

        return i4check(i4fu4(x));
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
    func1:conv1, 
    func2:conv2, 
    func3:conv3, 
    func4:conv4, 
    func5:conv5, 
    func6:conv6, 
    func7:conv7,
    func8:conv8
    };
}

var m = asmModule(this, {g1:SIMD.Uint32x4(0x55, 0x55, 0x55, 0x55), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

//  printSimdBaseline(m.func1(), "SIMD.Uint32x4", "m.func1()", "Func1");
// printSimdBaseline(m.func2(), "SIMD.Uint32x4", "m.func2()", "Func2");
// printSimdBaseline(m.func3(), "SIMD.Uint32x4", "m.func3()", "Func3");
// printSimdBaseline(m.func4(), "SIMD.Uint32x4", "m.func4()", "Func4");  
// printSimdBaseline(m.func5(), "SIMD.Uint32x4", "m.func5()", "Func5");
// printSimdBaseline(m.func6(), "SIMD.Uint32x4", "m.func6()", "Func6");  
// printSimdBaseline(m.func7(SIMD.Float32x4(0.5,1.4,2.4,5.5)), "SIMD.Uint32x4", "m.func7()", "Func7");
// printSimdBaseline(m.func8(), "SIMD.Uint32x4", "m.func8()", "Func8"); 

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.func1());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.func2());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.func3());
var ret4 = SIMD.Uint32x4.fromInt32x4Bits(m.func4());
var ret5 = SIMD.Uint32x4.fromInt32x4Bits(m.func5());
var ret6 = SIMD.Uint32x4.fromInt32x4Bits(m.func6());
var ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(0.5,1.4,2.4,5.5)));
var ret8 = SIMD.Uint32x4.fromInt32x4Bits(m.func8());

equalSimd([1167935898, 1163169792, 1143363011, 1190908979], ret1, SIMD.Uint32x4, "Func1")
equalSimd([3229614080, 3221225472, 3217031168, 3212836864],ret2, SIMD.Uint32x4, "Func2")
equalSimd([1, 2, 3, 4], ret3, SIMD.Uint32x4, "Func3")
equalSimd([0, 0, 0, 0], ret4, SIMD.Uint32x4, "Func4")
equalSimd([2863311530, 2863311530, 2863311530, 2863311530], ret5, SIMD.Uint32x4, "Func5")
equalSimd([1431655765, 1431655765, 1431655765, 1431655765], ret6, SIMD.Uint32x4, "Func6")

equalSimd([0, 1, 2, 5], ret7, SIMD.Uint32x4, "Func7")
ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(0.5,0.0,-.5,-.6)));
equalSimd([0, 0, 0, 0], ret7, SIMD.Uint32x4, "Func7")
ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(-0.5, 1.0, -0.0, -0.0)));
equalSimd([0, 1, 0, 0], ret7, SIMD.Uint32x4, "Func7");
ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(0.5,0.0,-.5,-.999)));
equalSimd([0, 0, 0, 0], ret7, SIMD.Uint32x4, "Func7")

equalSimd([11141290, 11141290, 11141290, 11141290], ret8, SIMD.Uint32x4, "Func8")

// passing:

//printSimdBaseline(m.func7(SIMD.Float32x4(0.5,1.4,2.4,5.5)), "SIMD.Uint32x4", "m.func7(SIMD.Float32x4(0.5,1.4,2.4,5.5))", "Func7_1");
//printSimdBaseline(m.func7(SIMD.Float32x4(0.5,0x80000000,0x100000000 - 0x81 /*2^32 - (2^7 + 1) rounded to 2^32 - 2^8*/ ,0)), "SIMD.Uint32x4", "m.func7(SIMD.Float32x4(0.5,0x80000000,0x100000000 - 0x81 /*2^32 - (2^7 + 1) rounded to 2^32 - 2^8*/ ,0))", "Func7_2");
//printSimdBaseline(m.func7(SIMD.Float32x4(-0.0, -0.0, -0.0, -0.0)), "SIMD.Uint32x4", "m.func7(SIMD.Float32x4(-0.0, -0.0, -0.0, -0.0))", "Func7_3");

var ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(0.5,1.4,2.4,5.5)));
var ret8 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(0.5,0x80000000,0x100000000 - 0x81 /*2^32 - (2^7 + 1) rounded to 2^32 - 2^8*/ ,0)));
var ret9 = SIMD.Uint32x4.fromInt32x4Bits(m.func7(SIMD.Float32x4(-0.0, -0.0, -0.0, -0.0)));

equalSimd([0, 1, 2, 5], ret7, SIMD.Uint32x4, "Func7_1")
equalSimd([0, 2147483648, 4294967040, 0], ret8, SIMD.Uint32x4, "Func7_2")
equalSimd([0, 0, 0, 0], ret9, SIMD.Uint32x4, "Func7_3")

// throwing
try 
{
    m.func7(SIMD.Float32x4(-0.0, -NaN, -0.0, -0.0));
    print("ERROR Func17_4");
} catch (e)
{
    // PASS
}
try 
{
    m.func7(SIMD.Float32x4(-0.0, Infinity, -0.0, -0.0));
    print("ERROR Func17_4");
} catch (e)
{
    // PASS
}
try 
{
    m.func7(SIMD.Float32x4(-0.0, -Infinity, -0.0, -0.0));
    print("ERROR Func17_4");
} catch (e)
{
    // PASS
}
try 
{
    m.func7(SIMD.Float32x4(-0.0, NaN, -0.0, -0.0));
    print("ERROR Func17_4");
} catch (e)
{
    // PASS
}

try 
{
    m.func7(SIMD.Float32x4(-1,0.0,-.5,-.999));
    print("ERROR Func17_5");
} catch (e)
{
    // PASS
}

try 
{
    m.func7(SIMD.Float32x4(-1112.5, 1.0, -0.0, -0.0));
    print("ERROR Func17_6");
} catch (e)
{
    // PASS
}

try 
{
    m.func7(SIMD.Float32x4(0.5,0x80000000,0x100000000 - 0x80 /*2^32 - (2^7) rounded to 2^32 */ ,0));
    print("ERROR Func17_8");
} catch (e)
{
    // PASS
}

var ret = SIMD.Uint32x4.fromInt32x4Bits(m.func8());
equalSimd([11141290, 11141290, 11141290, 11141290], ret, SIMD.Uint32x4, "Func8")

print("PASS");
