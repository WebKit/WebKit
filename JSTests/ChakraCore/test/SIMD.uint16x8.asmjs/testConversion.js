//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    
    var ui8fromFloat32x4Bits = ui8.fromFloat32x4Bits;
    var ui8fromInt16x8Bits = ui8.fromInt16x8Bits;
    var ui8fromInt32x4Bits = ui8.fromInt32x4Bits;
    var ui8fromUint32x4Bits = ui8.fromUint32x4Bits;
    var ui8fromInt8x16Bits = ui8.fromInt8x16Bits;   
    var ui8fromUint8x16Bits = ui8.fromUint8x16Bits;
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fromU8bits = i8.fromUint16x8Bits;
    var i16 = stdlib.SIMD.Int8x16;
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui4 = stdlib.SIMD.Uint32x4;
    
    var fround = stdlib.Math.fround;

    var globImportui8 = ui8check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized

    var loopCOUNT = 3;

    function conv1()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
       
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui8fromFloat32x4Bits(g1);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fromU8bits(x));
    }
    
    function conv2()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui8fromInt32x4Bits(globImportI4);
        }
        return i8check(i8fromU8bits(x));
    }

    function conv3()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
        var y = ui4(1,2,3,4);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = ui8fromUint32x4Bits(y);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i8check(i8fromU8bits(x));
    }

    function conv4()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
        var y = i8(1,2,3,4,5,6,7,8);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = ui8fromInt16x8Bits(y);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fromU8bits(x));
    }
    function conv5()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
        var m = i16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui8fromInt8x16Bits(m);
        }
        return i8check(i8fromU8bits(x));
    }
   
    function conv6()
    {
        var x = ui8(0,0,0,0,0,0,0,0);
        var m = ui16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = ui8fromUint8x16Bits(m);
        }
        return i8check(i8fromU8bits(x));
    }

    return {
    func1:conv1, 
    func2:conv2, 
    func3:conv3, 
    func4:conv4, 
    func5:conv5, 
    func6:conv6, 
    };
}

var m = asmModule(this, {g1:SIMD.Uint16x8(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.func1());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.func2());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.func3());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits(m.func4());
var ret5 = SIMD.Uint16x8.fromInt16x8Bits(m.func5());
var ret6 = SIMD.Uint16x8.fromInt16x8Bits(m.func6());

// printSimdBaseline(ret1, "SIMD.Uint16x8", "m.func1())", "Func1");
// printSimdBaseline(ret2, "SIMD.Uint16x8", "m.func2()", "Func2");
// printSimdBaseline(ret3, "SIMD.Uint16x8", "m.func3()", "Func3");
// printSimdBaseline(ret4, "SIMD.Uint16x8", "m.func4()", "Func4");
// printSimdBaseline(ret5, "SIMD.Uint16x8", "m.func5()", "Func5");
// printSimdBaseline(ret6, "SIMD.Uint16x8", "m.func6()", "Func6");

equalSimd([18842, 17821, 36864, 17748, 21955, 17446, 54323, 18171], ret1, SIMD.Uint16x8, "Func1");
equalSimd([0, 49280, 0, 49152, 0, 49088, 0, 49024], ret2, SIMD.Uint16x8, "Func2")
equalSimd([1, 0, 2, 0, 3, 0, 4, 0], ret3, SIMD.Uint16x8, "Func3")
equalSimd([1, 2, 3, 4, 5, 6, 7, 8], ret4, SIMD.Uint16x8, "Func4")
equalSimd([43690, 43690, 43690, 43690, 43690, 43690, 43690, 43690], ret5, SIMD.Uint16x8, "Func5")
equalSimd([21845, 21845, 21845, 21845, 21845, 21845, 21845, 21845], ret6, SIMD.Uint16x8, "Func6")


print("PASS");
