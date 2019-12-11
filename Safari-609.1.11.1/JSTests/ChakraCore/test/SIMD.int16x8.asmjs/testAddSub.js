//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8add = i8.add;
    var i8sub = i8.sub;
 
    var globImporti8 = i8check(imports.g1);     

    var i8g1 = i8(10, -1073, -107, 1082, 10402, 12332, 311, -650);      
    var i8g2 = i8(353216, -492529, -1128, 1085, 3692, 3937, 9755, 2638);     

    var loopCOUNT = 3;

    function testAddLocal()
    {
        var a = i8(5033,-3401, 665,-3234, -948, 2834, 7748, -25);
        var b = i8(-3483, 2144, -5697, 65, 1000000, -984, 3434, -9876);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8add(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    function testSubLocal()
    {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var b = i8(-3483, 2144, -5697, 65, 1000000, -984, 3434, -9876);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8sub(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }

    function testAddGlobal()
    {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8add(i8g1, i8g2);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(result);
    }
    
    function testSubGlobal()
    {
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8sub(i8g1, i8g2);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(result);
    }
    
    function testAddGlobalImport()
    {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = i8add(globImporti8, a);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(result);
    }
    
    function testSubGlobalImport()
    {
        var a = i8(5033, -3401, 665, -3234, -948, 2834, 7748, -25);
        var result = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = i8sub(globImporti8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(result);
    }
    
    return { testAddLocal: testAddLocal, testSubLocal: testSubLocal, testAddGlobal: testAddGlobal, testSubGlobal: testSubGlobal, testAddGlobalImport: testAddGlobalImport, testSubGlobalImport: testSubGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Int16x8(100, -1073741824, -1028, -102, 3883, -38, -92929, 1442)});

equalSimd([1550, -1257, -5032, -3169, 16012, 1850, 11182, -9901], m.testAddLocal(), SIMD.Int16x8, "Func1");
equalSimd([8516, -5545, 6362, -3299, -17908, 3818, 4314, 9851], m.testSubLocal(), SIMD.Int16x8, "Func2");
equalSimd([25546, 30686, -1235, 2167, 14094, 16269, 10066, 1988], m.testAddGlobal(), SIMD.Int16x8, "Func3");
equalSimd([-25526, 32704, 1021, -3, 6710, 8395, -9444, -3288], m.testSubGlobal(), SIMD.Int16x8, "Func4");
equalSimd([5133, -3401, -363, -3336, 2935, 2796, -19645, 1417], m.testAddGlobalImport(), SIMD.Int16x8, "Func5");
equalSimd([-4933, 3401, -1693, 3132, 4831, -2872, 30395, 1467], m.testSubGlobalImport(), SIMD.Int16x8, "Func6");
print("PASS");
