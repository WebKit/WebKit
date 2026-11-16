//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16splat = ui16.splat;
    var ui16fromFloat32x4Bits = ui16.fromFloat32x4Bits;
    var ui16fromInt32x4Bits = ui16.fromInt32x4Bits;
    var ui16fromUint32x4Bits = ui16.fromUint32x4Bits;
    var ui16fromInt16x8Bits = ui16.fromInt16x8Bits;
    var ui16fromUint16x8Bits = ui16.fromUint16x8Bits;
    var ui16fromInt8x16Bits = ui16.fromInt8x16Bits;
     
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;

    var i8 = stdlib.SIMD.Int16x8;
    var ui8 = stdlib.SIMD.Uint16x8;
    var i16 = stdlib.SIMD.Int8x16;
    var ui4 = stdlib.SIMD.Uint32x4;
    
    var fround = stdlib.Math.fround;

    var globImporti168 = ui16check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized
    var loopCOUNT = 3;

    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;

    function conv1()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
       
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui16fromFloat32x4Bits(g1);

            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fu16(x));
    }
    
    function conv2()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui16fromInt32x4Bits(globImportI4);
        }
        return i16check(i16fu16(x));
    }

    function conv3()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var y = ui4(1,2,3,4);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = ui16fromUint32x4Bits(y);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
       
        return i16check(i16fu16(x));
    }

    function conv4()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var y = ui8(1,2,3,4,5,6,7,8);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui16fromUint16x8Bits(y);

            loopIndex = (loopIndex + 1) | 0;
        }
       
        return i16check(i16fu16(x));
    }

    function conv5()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var m = i16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui16fromInt8x16Bits(m);
        }
        return i16check(i16fu16(x));
    }
    
    function conv6()
    {
        var x = ui16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
        var m = i8(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui16fromInt16x8Bits(m);
        }

        return i16check(i16fu16(x));
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
    func6:conv6
     };
}

var m = asmModule(this, {g1:SIMD.Uint8x16(0x55, 0x55, 0x55, 0x55,0x55, 0x55, 0x55, 0x55), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

/*printSimdBaseline(m.func1(), "SIMD.Uint8x16", "m.func1()", "Func1");
printSimdBaseline(m.func2(), "SIMD.Uint8x16", "m.func2()", "Func2");
printSimdBaseline(m.func3(), "SIMD.Uint8x16", "m.func3()", "Func3");
printSimdBaseline(m.func4(), "SIMD.Uint8x16", "m.func4()", "Func4");  
printSimdBaseline(m.func5(), "SIMD.Uint8x16", "m.func5()", "Func5");
printSimdBaseline(m.func6(), "SIMD.Uint8x16", "m.func6()", "Func6");  */

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.func1());
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.func2());
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.func3());
var ret4 = SIMD.Uint8x16.fromInt8x16Bits(m.func4());
var ret5 = SIMD.Uint8x16.fromInt8x16Bits(m.func5());
var ret6 = SIMD.Uint8x16.fromInt8x16Bits(m.func6());


equalSimd([154, 73, 157, 69, 0, 144, 84, 69, 195, 85, 38, 68, 51, 212, 251, 70], ret1, SIMD.Uint8x16, "Func1");
equalSimd([0, 0, 128, 192, 0, 0, 0, 192, 0, 0, 192, 191, 0, 0, 128, 191], ret2, SIMD.Uint8x16, "Func2");
equalSimd([1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0], ret3, SIMD.Uint8x16, "Func3");
equalSimd([1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0], ret4, SIMD.Uint8x16, "Func4");
equalSimd([170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170], ret5, SIMD.Uint8x16, "Func5");
equalSimd([170, 0, 170, 0, 170, 0, 170, 0, 170, 0, 170, 0, 170, 0, 170, 0], ret6, SIMD.Uint8x16, "Func6");

print("PASS");
