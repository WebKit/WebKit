//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var ui4 = stdlib.SIMD.Uint32x4;
    var ui4check = ui4.check;
    var ui4splat = ui4.splat;
    var ui4fromFloat32x4 = ui4.fromFloat32x4;
    var ui4fromFloat32x4Bits = ui4.fromFloat32x4Bits;
    var ui4fromInt32x4Bits = ui4.fromInt32x4Bits;
    var ui4fromInt16x8Bits = ui4.fromInt16x8Bits;
    var ui4fromUint16x8Bits = ui4.fromUint16x8Bits;
    var ui4fromInt8x16Bits = ui4.fromInt8x16Bits;
    var ui4fromUint8x16Bits = ui4.fromUint8x16Bits;;
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;

    var i8 = stdlib.SIMD.Int16x8;
    var u8 = stdlib.SIMD.Uint16x8;
    
    var i16 = stdlib.SIMD.Int8x16;
    var u16 = stdlib.SIMD.Uint8x16;
    
    var fround = stdlib.Math.fround;

    var globImporti168 = ui4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized
    var loopCOUNT = 3;
    
    var i4fu4 = i4.fromUint32x4Bits;

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
            x = ui4fromInt32x4Bits(globImportI4);
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

            x = ui4fromInt32x4Bits(y);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
       
        return i4check(i4fu4(x));
    }

    function conv4()
    {
        var x = ui4(0,0,0,0);
        var y = u8(1,2,3,4,5,6,7,8);
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
        var m = u16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui4fromUint8x16Bits(m);
        }
        return i4check(i4fu4(x));
    }

    function conv7()
    {
        var x = ui4(0,0,0,0);
        var y = f4(1034.0, 22342.0,1233.0, 40443.0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui4fromFloat32x4(y);

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

/*printSimdBaseline(m.func1(), "SIMD.Uint32x4", "m.func1()", "Func1");
printSimdBaseline(m.func2(), "SIMD.Uint32x4", "m.func2()", "Func2");
printSimdBaseline(m.func3(), "SIMD.Uint32x4", "m.func3()", "Func3");
//printSimdBaseline(m.func4(), "SIMD.Uint32x4", "m.func4()", "Func4");  // need to check exception is thrown
printSimdBaseline(m.func5(), "SIMD.Uint32x4", "m.func5()", "Func5");
//printSimdBaseline(m.func6(), "SIMD.Uint32x4", "m.func6()", "Func6");  //need to check exception is thrown
printSimdBaseline(m.func7(), "SIMD.Uint32x4", "m.func7()", "Func7");
printSimdBaseline(m.func8(), "SIMD.Uint32x4", "m.func8()", "Func8");*/

var ret1 = SIMD.Uint32x4.fromInt32x4Bits(m.func1());
var ret2 = SIMD.Uint32x4.fromInt32x4Bits(m.func2());
var ret3 = SIMD.Uint32x4.fromInt32x4Bits(m.func3());
var ret5 = SIMD.Uint32x4.fromInt32x4Bits(m.func5());
var ret7 = SIMD.Uint32x4.fromInt32x4Bits(m.func7());
var ret8 = SIMD.Uint32x4.fromInt32x4Bits(m.func8());

equalSimd([1167935898, 1163169792, 1143363011, 1190908979], ret1, SIMD.Uint32x4, "Func1");
equalSimd([3229614080, 3221225472, 3217031168, 3212836864], ret2, SIMD.Uint32x4, "Func2");
equalSimd([1, 2, 3, 4], ret3, SIMD.Uint32x4, "Func3");
equalSimd([2863311530, 2863311530, 2863311530, 2863311530], ret5, SIMD.Uint32x4, "Func5");
equalSimd([1034, 22342, 1233, 40443], ret7, SIMD.Uint32x4, "Func7");
equalSimd([11141290, 11141290, 11141290, 11141290], ret8, SIMD.Uint32x4, "Func8");


print("PASS");
