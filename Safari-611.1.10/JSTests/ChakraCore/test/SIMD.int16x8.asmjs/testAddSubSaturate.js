//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8addSaturate = i8.addSaturate;
    var i8subSaturate = i8.subSaturate;
 
    var globImporti8 = i8check(imports.g1);     

    var i8g1 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1040103192, 1234123832, 0807627311, 0659275670);      
    var i8g2 = i8(353216, -492529, -1128, 1085, 3692, 3937, 9755, 2638);     

    var loopCOUNT = 3;

    function testAddSaturateLocal()
    {
        var a = i8(32765, 32766, 32767, 32767, 1, 1, 1, 1);
        var b = i8(1, 1, 1, 5000, 1, 1, 1, 1);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8addSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testSubSaturateLocal()
    {
        var a = i8(-100, -3401, 665, -3234, -948, 2834, 7748, -25);
        var b = i8(32767, 2144, -5697, 65, 1000000, -984, 3434, -9876);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8subSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testAddSaturateGlobal()
    {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8addSaturate(i8g1, i8g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testSubSaturateGlobal()
    {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8subSaturate(i8g1, i8g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testAddSaturateGlobalImport()
    {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8addSaturate(globImporti8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testSubSaturateGlobalImport()
    {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8subSaturate(globImporti8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    return { testAddSaturateLocal: testAddSaturateLocal, testSubSaturateLocal: testSubSaturateLocal, testAddSaturateGlobal: testAddSaturateGlobal, testSubSaturateGlobal: testSubSaturateGlobal, testAddSaturateGlobalImport: testAddSaturateGlobalImport, testSubSaturateGlobalImport: testSubSaturateGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Int16x8(100, -1073741824, -1028, -102, 3883, -38, -92929, 1442)});

equalSimd([32766, 32767, 32767, 32767, 2, 2, 2, 2], m.testAddSaturateLocal(), SIMD.Int16x8, "Func1");
equalSimd([-32768, -5545, 6362, -3299, -17908, 3818, 4314, 9851], m.testSubSaturateLocal(), SIMD.Int16x8, "Func2");
equalSimd([25536, 31759, -1128, 1085, -14972, 19353, 32767, -13852], m.testAddSaturateGlobal(), SIMD.Int16x8, "Func3");
equalSimd([-25536, -31759, 1128, -1085, -22356, 11479, 17428, -19128], m.testSubSaturateGlobal(), SIMD.Int16x8, "Func4");
equalSimd([5133, -3401, -363, -3336, 2935, 2796, -19645, 1417], m.testAddSaturateGlobalImport(), SIMD.Int16x8, "Func5");
equalSimd([-4933, 3401, -1693, 3132, 4831, -2872, -32768, 1467], m.testSubSaturateGlobalImport(), SIMD.Int16x8, "Func6");
print("PASS");
