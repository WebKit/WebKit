//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b16 = stdlib.SIMD.Bool8x16;
    var b16check = b16.check;
    var b16allTrue = b16.allTrue;
    var b16anyTrue = b16.anyTrue;
 
    var globImportb16 = b16check(imports.g1);     

    var b16g1 = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    var loopCOUNT = 5;

    function testAnyTrue()
    {
        var a = b16(1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            result = b16anyTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    function testAllTrue()
    {
        var a = b16(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0))
        {
            result = b16allTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    return {testAnyTrue:testAnyTrue, testAllTrue:testAllTrue};
}

var m = asmModule(this, { g1: SIMD.Bool8x16(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) });

equal(1, m.testAnyTrue());
equal(1, m.testAllTrue());
print("PASS");
