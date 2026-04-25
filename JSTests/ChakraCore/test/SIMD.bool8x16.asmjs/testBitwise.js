//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b16 = stdlib.SIMD.Bool8x16;
    var b16check = b16.check;
    var b16and = b16.and;
    var b16or = b16.or;
    var b16not = b16.not;
    var b16xor = b16.xor;
 
    var globImportb16 = b16check(imports.g1);     

    var b16g1 = b16(1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0);

    var loopCOUNT = 5;

    function testBitwiseAND()
    {
        var a = b16(1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            result = b16and(a, b16g1);
            result = b16and(result, globImportb16);

            b16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b16check(result);
    }

    function testBitwiseOR() {
        var a = b16(1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b16or(a, b16g1);
            result = b16or(result, globImportb16);

            b16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b16check(result);
    }

    function testBitwiseXOR() {
        var a = b16(1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b16xor(a, b16g1);
            result = b16xor(result, globImportb16);

            b16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b16check(result);
    }

    function testBitwiseNOT() {
        var a = b16(1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b16and(a, b16g1);
            result = b16and(result, globImportb16);

            b16check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b16check(result);
    }

    return {testBitwiseAND:testBitwiseAND, testBitwiseOR,testBitwiseOR, testBitwiseXOR:testBitwiseXOR, testBitwiseNOT:testBitwiseNOT};
}

var m = asmModule(this, { g1: SIMD.Bool8x16(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1) });

equalSimd([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m.testBitwiseAND(), SIMD.Bool8x16, "Func1");
equalSimd([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], m.testBitwiseOR(), SIMD.Bool8x16, "Func2");
equalSimd([0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0], m.testBitwiseXOR(), SIMD.Bool8x16, "Func3");
equalSimd([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m.testBitwiseNOT(), SIMD.Bool8x16, "Func4");
print("PASS");
