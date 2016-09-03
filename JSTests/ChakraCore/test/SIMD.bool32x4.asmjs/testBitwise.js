//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b4 = stdlib.SIMD.Bool32x4;
    var b4check = b4.check;
    var b4and = b4.and;
    var b4or = b4.or;
    var b4not = b4.not;
    var b4xor = b4.xor;
 
    var globImportb4 = b4check(imports.g1);     

    var b4g1 = b4(1,1,1,1); 

    var loopCOUNT = 5;

    function testBitwiseAND()
    {
        var a = b4(1, 0, 1, 1);
        var result = b4(0, 0, 0, 0);
        
        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            result = b4and(b4g1, b4g1);
            result = b4and(result, globImportb4);

            b4check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b4check(b4g1);
    }
    
    function testBitwiseOR() {
        var a = b4(1, 0, 1, 1);
        var result = b4(0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b4or(a, b4g1);
            result = b4or(result, globImportb4);

            b4check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b4check(result);
    }

    function testBitwiseXOR() {
        var a = b4(1, 0, 1, 1);
        var result = b4(0, 0, 0, 0);

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b4xor(a, b4g1);
            result = b4xor(result, globImportb4);

            b4check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b4check(result);
    }

    function testBitwiseNOT() {
        var a = b4(1, 0, 1, 1);
        var result = b4(0, 0, 0, 0);
        var loopIndex = 0;

        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b4and(a, b4g1);
            result = b4and(result, globImportb4);

            b4check(result);

            loopIndex = (loopIndex + 1) | 0;
        }

        return b4check(result);
    }

    return {testBitwiseAND:testBitwiseAND, testBitwiseOR,testBitwiseOR, testBitwiseXOR:testBitwiseXOR, testBitwiseNOT:testBitwiseNOT};
}

var m = asmModule(this, {g1:SIMD.Bool32x4(1, 0, 1, 0)});

equalSimd([1, 1, 1, 1], m.testBitwiseAND(), SIMD.Bool32x4, "Func1");
equalSimd([1, 1, 1, 1], m.testBitwiseOR(), SIMD.Bool32x4, "Func2");
equalSimd([1, 1, 1, 0], m.testBitwiseXOR(), SIMD.Bool32x4, "Func3");
equalSimd([1, 0, 1, 0], m.testBitwiseNOT(), SIMD.Bool32x4, "Func4");
print("PASS");
