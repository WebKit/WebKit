//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var ui8 = stdlib.SIMD.Uint16x8;
    var ui8check = ui8.check;
    var ui8addSaturate = ui8.addSaturate;
    var ui8subSaturate = ui8.subSaturate;
 
    var globImportui8 = ui8check(imports.g1);     

    var ui8g1 = ui8(1065353216, 1073741824, 1077936128, 1082130432, 1040103192, 1234123832, 0807627311, 0659275670);      
    var ui8g2 = ui8(353216, 492529, 1128, 1085, 3692, 3937, 9755, 2638);     

    var loopCOUNT = 3;
    
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fu8 = i8.fromUint16x8Bits;
    var u8fi8 = ui8.fromInt16x8Bits;
    
    function testAddSaturateLocal()
    {
        var a = ui8(50000, 65535, 0, 65535, 1, 1, 1, 1);
        var b = ui8(50000, 32766, 65535, 1, 1, 1, 1, 1);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8addSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testSubSaturateLocal()
    {
        var a = ui8(100, 3401, 665, 3234, 948, 2834, 7748, 25);
        var b = ui8(32767, 2144, 5697, 65, 60000, 984, 3434, 9876);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8subSaturate(a, b);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }

    function testAddSaturateGlobal()
    {
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8addSaturate(ui8g1, ui8g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testSubSaturateGlobal()
    {
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8subSaturate(ui8g1, ui8g2);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testAddSaturateGlobalImport()
    {
        var a = ui8(50330, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            result = ui8addSaturate(globImportui8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    function testSubSaturateGlobalImport()
    {
        var a = ui8(50330, 3401, 665, 3234, 948, 2834, 7748, 25);
        var result = ui8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = ui8subSaturate(globImportui8, a);
            loopIndex = (loopIndex + 1) | 0;
        }

        return i8check(i8fu8(result));
    }
    
    return { testAddSaturateLocal: testAddSaturateLocal, testSubSaturateLocal: testSubSaturateLocal, testAddSaturateGlobal: testAddSaturateGlobal, testSubSaturateGlobal: testSubSaturateGlobal, testAddSaturateGlobalImport: testAddSaturateGlobalImport, testSubSaturateGlobalImport: testSubSaturateGlobalImport };
}

var m = asmModule(this, {g1:SIMD.Uint16x8(20000, 1073741824, 1028, 102, 3883, 38, 92929, 1442)});

var ret1 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddSaturateLocal());
var ret2 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubSaturateLocal());
var ret3 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddSaturateGlobal());
var ret4 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubSaturateGlobal());
var ret5 = SIMD.Uint16x8.fromInt16x8Bits(m.testAddSaturateGlobalImport());
var ret6 = SIMD.Uint16x8.fromInt16x8Bits(m.testSubSaturateGlobalImport());


equalSimd([65535, 65535, 65535, 65535, 2, 2, 2, 2], ret1, SIMD.Uint16x8, "Func1");
equalSimd([0, 1257, 0, 3169, 0, 1850, 4314, 0], ret2, SIMD.Uint16x8, "Func2");
equalSimd([25536, 33777, 1128, 1085, 50564, 19353, 36938, 51684], ret3, SIMD.Uint16x8, "Func3");
equalSimd([0, 0, 0, 0, 43180, 11479, 17428, 46408], ret4, SIMD.Uint16x8, "Func4");
equalSimd([65535, 3401, 1693, 3336, 4831, 2872, 35141, 1467], ret5, SIMD.Uint16x8, "Func5");
equalSimd([0, 0, 363, 0, 2935, 0, 19645, 1417], ret6, SIMD.Uint16x8, "Func6");
print("PASS");
