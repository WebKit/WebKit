//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8add = ui8.add;
    var ui8sub = ui8.sub;
 
    var globImportui8 = ui8check(imports.g1);     

    var ui8g1 = ui8(10, 1073, 107, 1082, 10402, 12332, 311, 650);      
    var ui8g2 = ui8(353216, 492529, 1128, 1085, 3692, 3937, 9755, 2638);     

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;
    var u8fi8 = ui8.fromInt16x8Bits;

    var loopCOUNT = 3;

    function testAddLocal()
    {
        var a = ui8(5033,3401, 665,3234, 948, 2834, 7748, 25);
        var b = ui8(3483, 2144, 5697, 65, 1000000, 984, 3434, 9876);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8add(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testSubLocal()
    {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var b = ui8(3483, 2144, 5697, 65, 1000000, 984, 3434, 9876);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8sub(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }

    function testAddGlobal()
    {
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8add(ui8g1, ui8g2);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(i8fu8(result));
    }
    
    function testSubGlobal()
    {
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8sub(ui8g1, ui8g2);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(i8fu8(result));
    }
    
    function testAddGlobalImport()
    {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8add(globImportui8, a);
        loopIndex = (loopIndex + 1) | 0;
    }

        return i8check(i8fu8(result));
    }
    
    function testSubGlobalImport()
    {
        var a = ui8(5033, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8sub(globImportui8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    return { testAddLocal: testAddLocal, testSubLocal: testSubLocal, testAddGlobal: testAddGlobal, testSubGlobal: testSubGlobal, testAddGlobalImport: testAddGlobalImport, testSubGlobalImport: testSubGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Uint16x8(100, 1073741824, 1028, 102, 3883, 38, 92929, 1442)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddLocal());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubLocal());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddGlobal());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubGlobal());
var ret5 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddGlobalImport());
var ret6 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubGlobalImport());

equalSimd([8516, 5545, 6362, 3299, 17908, 3818, 11182, 9901], ret1, SIMD.Uint16x8, "Func1");
equalSimd([1550, 1257, 60504, 3169, 49524, 1850, 4314, 55685],ret2, SIMD.Uint16x8, "Func2");
equalSimd([25546, 34850, 1235, 2167, 14094, 16269, 10066, 3288], ret3, SIMD.Uint16x8, "Func3");
equalSimd([40010, 32832, 64515, 65533, 6710, 8395, 56092, 63548],ret4, SIMD.Uint16x8, "Func4");
equalSimd([5133, 3401, 1693, 3336, 4831, 2872, 35141, 1467], ret5, SIMD.Uint16x8, "Func5");
equalSimd([60603, 62135, 363, 62404, 2935, 62740, 19645, 1417], ret6, SIMD.Uint16x8, "Func6");
print("PASS");
