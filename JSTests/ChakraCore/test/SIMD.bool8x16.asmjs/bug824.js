//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//https://github.com/Microsoft/ChakraCore/issues/824

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b16 = stdlib.SIMD.Bool8x16;
    var b16extractLane = b16.extractLane;
    var b16replaceLane = b16.replaceLane;
    var b16check = b16.check;

 function testExtractLane() {

        var a = b16(1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        
        var l0 = 0;
        var l1 = 1;
        var l4 = 0;
        var l6 = 0;
        
        l0 = b16extractLane(a, 0)|0;
        l1 = b16extractLane(a, 1)|0; //0
        l4 = b16extractLane(a, 4)|0;
        l6 = b16extractLane(a, 6)|0;
        
    
        result = b16replaceLane(result, 0, l0);
        result = b16replaceLane(result, 5, l4);
        result = b16replaceLane(result, 10, l6);
        result = b16replaceLane(result, 15, l1);
        
        return b16check(result); 
    }

    return {testExtractLane:testExtractLane};
}

var m = asmModule(this, {});

equalSimd([true, false, false, false, false, true, false, false, false, false, true, false, false, false, false, false], m.testExtractLane(), SIMD.Bool8x16, "testExtractLane");

print("PASS");
