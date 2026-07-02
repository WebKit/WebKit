//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16add = i16.add;
    var i16sub = i16.sub;
 
    var globImporti16 = i16check(imports.g1);     

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);      
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 0, 0);     

    var loopCOUNT = 3;

    function testAddLocal()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16add(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubLocal()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16sub(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testAddGlobal()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16add(i16g1, i16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubGlobal()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16sub(i16g1, i16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testAddGlobalImport()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16add(globImporti16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubGlobalImport()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16sub(globImporti16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return { testAddLocal: testAddLocal, testSubLocal: testSubLocal, testAddGlobal: testAddGlobal, testSubGlobal: testSubGlobal, testAddGlobalImport: testAddGlobalImport, testSubGlobalImport: testSubGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Int8x16(100, -1073741824, -1028, -102, 3883, -38, -92929, 1442, 52, 127, -127, -129, 129, 0, 88, 100234)});

equalSimd([-15, -13, -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11, 13, 15], m.testAddLocal(), SIMD.Int8x16, "Func1");
equalSimd([17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17], m.testSubLocal(), SIMD.Int8x16, "Func2");
equalSimd([-127, -126, -125, -124, 5, 6, -17, 32, 14, 25, 8, 125, 56, 35, 15, 16], m.testAddGlobal(), SIMD.Int8x16, "Func3");
equalSimd([-127, -126, -125, -124, 5, 6, 31, -16, 4, -5, 14, -101, -30, -7, 15, 16], m.testSubGlobal(), SIMD.Int8x16, "Func4");
equalSimd([101, 2, -1, -98, 48, -32, 6, -86, 61, -119, -116, -117, -114, 14, 103, -102], m.testAddGlobalImport(), SIMD.Int8x16, "Func5");
equalSimd([99, -2, -7, -106, 38, -44, -8, -102, 43, 117, 118, 115, 116, -14, 73, 122], m.testSubGlobalImport(), SIMD.Int8x16, "Func6");
print("PASS");
