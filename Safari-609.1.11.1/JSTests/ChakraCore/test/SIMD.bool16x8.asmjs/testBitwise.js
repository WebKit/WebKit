//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b8 = stdlib.SIMD.Bool16x8;
    var b8check = b8.check;
    var b8and = b8.and;
    var b8or = b8.or;
    var b8not = b8.not;
    var b8xor = b8.xor;

    var globImportb8 = b8check(imports.g1);     

    var b8g1 = b8(1, 0, 1, 0, 1, 0, 1, 0);

    var loopCOUNT = 5;

    function testBitwiseAND()
    {
        var a = b8(1, 0, 1, 1, 1, 0, 1, 1);
        var result = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            result = b8and(a, b8g1);
            result = b8and(result, globImportb8);

            b8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b8check(result);
    }

    function testBitwiseOR() {
        var a = b8(1, 0, 1, 1, 1, 0, 1, 1);
        var result = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b8or(a, b8g1);
            result = b8or(result, globImportb8);

            b8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b8check(result);
    }

    function testBitwiseXOR() {
        var a = b8(1, 0, 1, 1, 1, 0, 1, 1);
        var result = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b8xor(a, b8g1);
            result = b8xor(result, globImportb8);

            b8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b8check(result);
    }

    function testBitwiseNOT() {
        var a = b8(1, 0, 1, 1, 1, 0, 1, 1);
        var result = b8(0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b8and(a, b8g1);
            result = b8and(result, globImportb8);

            b8check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b8check(result);
    }

    return {testBitwiseAND:testBitwiseAND, testBitwiseOR,testBitwiseOR, testBitwiseXOR:testBitwiseXOR, testBitwiseNOT:testBitwiseNOT};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(0, 1, 0, 1, 0, 1, 0, 1)});

equalSimd([0, 0, 0, 0, 0, 0, 0, 0], m.testBitwiseAND(), SIMD.Bool16x8, "Func1");
equalSimd([1, 1, 1, 1, 1, 1, 1, 1], m.testBitwiseOR(), SIMD.Bool16x8, "Func2");
equalSimd([0, 1, 0, 0, 0, 1, 0, 0], m.testBitwiseXOR(), SIMD.Bool16x8, "Func3");
equalSimd([0, 0, 0, 0, 0, 0, 0, 0], m.testBitwiseNOT(), SIMD.Bool16x8, "Func4");
print("PASS");
