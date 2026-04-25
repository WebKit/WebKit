//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b4 = stdlib.SIMD.Bool32x4;
    var b4check = b4.check;
    var b4allTrue = b4.allTrue;
    var b4anyTrue = b4.anyTrue;
 
    var globImportb4 = b4check(imports.g1);     

    var b4g1 = b4(0, 0, 0, 0);

    var loopCOUNT = 5;

    function testAnyTrue() {
        var a = b4(1, 0, 1, 0);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b4anyTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    function testAllTrue()
    {
        var a = b4(1, 1, 1, 1);
        var result = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            result = b4allTrue(a) | 0;

            loopIndex = (loopIndex + 1) | 0;
        }

        return result | 0;
    }

    return {testAnyTrue:testAnyTrue, testAllTrue:testAllTrue};
}

var m = asmModule(this, {g1:SIMD.Bool32x4(1, 0, 1, 1)});

equal(1, m.testAnyTrue());
equal(1, m.testAllTrue());
print("PASS");
