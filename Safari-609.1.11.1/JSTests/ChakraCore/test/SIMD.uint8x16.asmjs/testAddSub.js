//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16add = ui16.add;
    var ui16sub = ui16.sub;
 
    var globImportui16 = ui16check(imports.g1);     

    var ui16g1 = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);      
    var ui16g2 = ui16(256, 255, 128, 127, 0, 0, 1000, 1000, 5, 15, 3, 399, 299, 21, 45, 22);

    var loopCOUNT = 3;
        
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;

    function testAddLocal()
    {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16add(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    function testSubLocal()
    {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = ui16(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16sub(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }

    function testAddGlobal()
    {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16add(ui16g1, ui16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    function testSubGlobal()
    {
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16sub(ui16g1, ui16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    function testAddGlobalImport()
    {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16add(globImportui16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    function testSubGlobalImport()
    {
        var a = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui16sub(globImportui16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(i16fu16(result));
    }
    
    return { testAddLocal: testAddLocal, testSubLocal: testSubLocal, testAddGlobal: testAddGlobal, testSubGlobal: testSubGlobal, testAddGlobalImport: testAddGlobalImport, testSubGlobalImport: testSubGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Uint8x16(100, 65535, 1028, 102, 3883, 38, 52929, 1442, 52, 127, 127, 129, 129, 0, 88, 10234)});

var ret1 = SIMD.Uint8x16.fromInt8x16Bits(m.testAddLocal());
var ret2 = SIMD.Uint8x16.fromInt8x16Bits(m.testSubLocal());
var ret3 = SIMD.Uint8x16.fromInt8x16Bits(m.testAddGlobal());
var ret4 = SIMD.Uint8x16.fromInt8x16Bits(m.testSubGlobal());
var ret5 = SIMD.Uint8x16.fromInt8x16Bits(m.testAddGlobalImport());
var ret6 = SIMD.Uint8x16.fromInt8x16Bits(m.testSubGlobalImport());


equalSimd([17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17], ret1, SIMD.Uint8x16, "Func1");
equalSimd([241, 243, 245, 247, 249, 251, 253, 255, 1, 3, 5, 7, 9, 11, 13, 15], ret2, SIMD.Uint8x16, "Func2");
equalSimd([1, 1, 131, 131, 5, 6, 239, 240, 14, 25, 14, 155, 56, 35, 60, 38], ret3, SIMD.Uint8x16, "Func3");
equalSimd([1, 3, 131, 133, 5, 6, 31, 32, 4, 251, 8, 125, 226, 249, 226, 250], ret4, SIMD.Uint8x16, "Func4");
equalSimd([101, 1, 7, 106, 48, 44, 200, 170, 61, 137, 138, 141, 142, 14, 103, 10], ret5, SIMD.Uint8x16, "Func5");
equalSimd([99, 253, 1, 98, 38, 32, 186, 154, 43, 117, 116, 117, 116, 242, 73, 234], ret6, SIMD.Uint8x16, "Func6");
print("PASS");
