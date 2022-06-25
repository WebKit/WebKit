//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b8 = stdlib.SIMD.Bool16x8;
    var b8check = b8.check;
    var b8allTrue = b8.allTrue;
    var b8anyTrue = b8.anyTrue;
 
    var globImportb8 = b8check(imports.g1);     

    var b8g1 = b8(0, 0, 0, 0, 0, 0, 0, 0);

    var loopCOUNT = 5;

    function testAnyTrue()
    {
        var a = b8(1, 0, 1, 0, 1, 0, 1, 0);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b8anyTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    function testAllTrue()
    {
        var a = b8(1, 1, 1, 1, 1, 1, 1, 1);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b8allTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    return {testAnyTrue:testAnyTrue, testAllTrue:testAllTrue};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(1, 1, 1, 1, 1, 1, 1, 1)});

equal(1, m.testAnyTrue());
equal(1, m.testAllTrue());
print("PASS");
