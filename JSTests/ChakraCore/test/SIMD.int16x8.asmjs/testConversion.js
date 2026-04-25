//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    
    var i8fromFloat32x4Bits = i8.fromFloat32x4Bits;
    var i8fromUint16x8Bits = i8.fromUint16x8Bits;
    var i8fromInt32x4Bits = i8.fromInt32x4Bits;
    var i8fromUint32x4Bits = i8.fromUint32x4Bits;
    //var i8fromInt8x16Bits = i8.fromInt8x16Bits;   
    var i8fromUint8x16Bits = i8.fromUint8x16Bits;
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui4 = stdlib.SIMD.Uint32x4;
    var ui16 = stdlib.SIMD.Uint8x16;
    //var i16 = stdlib.SIMD.Int8x16;

    var fround = stdlib.Math.fround;

    var globImporti8 = i8check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    var g1 = f4(5033.2,3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, 1073741824,1077936128, 1082130432);          // global var initialized
 
    var loopCOUNT = 3;

    function conv1()
    {
        var x = i8(0,0,0,0,0,0,0,0);
       
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i8fromFloat32x4Bits(g1);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(x);
    }
    
    function conv2()
    {
        var x = i8(0,0,0,0,0,0,0,0);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = i8fromInt32x4Bits(globImportI4);
        }
        return i8check(x);
    }

    function conv3()
    {
        var x = i8(0,0,0,0,0,0,0,0);
        var y = ui4(1,2,3,4);
        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            x = i8fromUint32x4Bits(y);

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return i8check(x);
    }

    function conv4()
    {
        var x = i8(0,0,0,0,0,0,0,0);
        var y = ui8(1,2,3,4,5,6,7,8);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i8fromUint16x8Bits(y);

            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(x);
    }
/*
    function conv5()
    {
        var x = i8(0,0,0,0,0,0,0,0);
        var m = i16(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = i8fromInt8x16Bits(m);
        }
        return i8check(x);
    }
  */  
    function conv6()
    {
        var x = i8(0,0,0,0,0,0,0,0);
        var m = ui16(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = i8fromUint8x16Bits(m);
        }
        return i8check(x);
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
    //func5:conv5, 
    func6:conv6, 
    };
}

var m = asmModule(this, {g1:SIMD.Int16x8(0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)});

/*printSimdBaseline(m.func1(), "SIMD.Int16x8", "m.func1())", "Func1");
printSimdBaseline(m.func2(), "SIMD.Int16x8", "m.func2()", "Func2");
printSimdBaseline(m.func3(), "SIMD.Int16x8", "m.func3()", "Func3");
printSimdBaseline(m.func4(), "SIMD.Int16x8", "m.func4()", "Func4");
printSimdBaseline(m.func5(), "SIMD.Int16x8", "m.func5()", "Func5");
printSimdBaseline(m.func6(), "SIMD.Int16x8", "m.func6()", "Func6");*/

equalSimd([18842, 17821, -28672, 17748, 21955, 17446, -11213, 18171], m.func1(), SIMD.Int16x8, "Func1")
equalSimd([0, -16256, 0, -16384, 0, -16448, 0, -16512], m.func2(), SIMD.Int16x8, "Func2")
equalSimd([1, 0, 2, 0, 3, 0, 4, 0], m.func3(), SIMD.Int16x8, "Func3")
equalSimd([1, 2, 3, 4, 5, 6, 7, 8], m.func4(), SIMD.Int16x8, "Func4")
//equalSimd([-21846, -21846, -21846, -21846, -21846, -21846, -21846, -21846], m.func5(), SIMD.Int16x8, "Func5")
equalSimd([21845, 21845, 21845, 21845, 21845, 21845, 21845, 21845], m.func6(), SIMD.Int16x8, "Func6")

print("PASS");
