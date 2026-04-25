//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16addSaturate = i16.addSaturate;
    var i16subSaturate = i16.subSaturate;
 
    var globImporti16 = i16check(imports.g1);     

    var i16g1 = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);      
    var i16g2 = i16(128, 128, -128, -128, 0, 0, 1000, -1000, 5, 15, -3, -399, 299, 21, 0, 0);     

    var loopCOUNT = 3;

    function testAddSaturateLocal()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(100, 100, -100, -100, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16addSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubSaturateLocal()
    {
        var a = i16(-40, 40, -40, 40, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var b = i16(100, 100, -100, -100, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16subSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }

    function testAddSaturateGlobal()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16addSaturate(i16g1, i16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubSaturateGlobal()
    {
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16subSaturate(i16g1, i16g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testAddSaturateGlobalImport()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16addSaturate(globImporti16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    function testSubSaturateGlobalImport()
    {
        var a = i16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var result = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i16subSaturate(globImporti16, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i16check(result);
    }
    
    return { testAddSaturateLocal: testAddSaturateLocal, testSubSaturateLocal: testSubSaturateLocal, testAddSaturateGlobal: testAddSaturateGlobal, testSubSaturateGlobal: testSubSaturateGlobal, testAddSaturateGlobalImport: testAddSaturateGlobalImport, testSubSaturateGlobalImport: testSubSaturateGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Int8x16(100, -1073741824, -1028, -102, 3883, -38, -92929, 1442, 52, 127, -127, -129, 129, 0, 88, 100234)});

equalSimd([60, 127, -128, -60, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11, 13, 15], m.testAddSaturateLocal(), SIMD.Int8x16, "Func1");
equalSimd([-128, -60, 60, 127, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17], m.testSubSaturateLocal(), SIMD.Int8x16, "Func2");
equalSimd([-127, -126, -125, -124, 5, 6, -17, 32, 14, 25, 8, 125, 56, 35, 15, 16], m.testAddSaturateGlobal(), SIMD.Int8x16, "Func3");
equalSimd([127, 127, 127, 127, 5, 6, 31, -16, 4, -5, 14, -101, -30, -7, 15, 16], m.testSubSaturateGlobal(), SIMD.Int8x16, "Func4");
equalSimd([101, 2, -1, -98, 48, -32, 6, -86, 61, 127, -116, 127, -114, 14, 103, -102], m.testAddSaturateGlobalImport(), SIMD.Int8x16, "Func5");
equalSimd([99, -2, -7, -106, 38, -44, -8, -102, 43, 117, -128, 115, -128, -14, 73, -128], m.testSubSaturateGlobalImport(), SIMD.Int8x16, "Func6");
print("PASS");
