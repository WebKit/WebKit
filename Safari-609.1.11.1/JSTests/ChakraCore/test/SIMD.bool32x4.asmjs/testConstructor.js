//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b4 = stdlib.SIMD.Bool32x4;
    var i4 = stdlib.SIMD.Int32x4;
    var i4lessThan = i4.lessThan;
    var b4check = b4.check;
    var b4splat = b4.splat;
    var b4and   = b4.and;
    var b4xor   = b4.xor;
    var b4extractLane = b4.extractLane;
    var b4replaceLane = b4.replaceLane;
 
    var globImportb4 = b4check(imports.g1);     
    var b4g1 = b4(0, 0, 0, 0);
    var loopCOUNT = 5;

    function testConstructor() {
        var a = b4(0, 0, 0, 0);
        var b = b4(0, 0, 0, 0);
        var i1 = i4(1, 0, 1, 8);
        var i2 = i4(12,13,0, 1);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b4(10*10,0,-1,loopIndex);
            b = i4lessThan(i1,i2);
            a = b4and(a, b);
        }
        return b4check(a);
    }
    
    function testSplat() {
        var a = b4(0, 0, 0, 0);
        var b = b4(0, 0, 0, 0);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b4splat(3);
            b = b4splat((3-3));
            a = b4xor(a, b);
        }
        return b4check(a);
    }
    
    function testLaneAccess() {
        var a = b4(5, 0, 1, 0);
        var result = b4(0, 0, 0, 0);
        var m = 0;
        var n = 0;
        var o = 0;
        var p = 0;

        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            m = b4extractLane(a, 0)|0;
            n = b4extractLane(a, 1)|0;
            o = b4extractLane(a, 2)|0;
            p = b4extractLane(a, 3)|0;
            result = b4replaceLane(a, 0, p);
            result = b4replaceLane(result, 1, o);
            result = b4replaceLane(result, 2, n);
            result = b4replaceLane(result, 3, m);
        }
        return b4check(result);
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret1 = 0;
        var a = b4(1,2,3,4);
        ret1 = (b4extractLane(a, 0))|0;
    }

    return {testConstructor:testConstructor,
    testSplat:testSplat,
    testLaneAccess: testLaneAccess};
}

var m = asmModule(this, {g1:SIMD.Bool32x4(1, 0, 1, 1)});

equalSimd([true, false, false, false], m.testConstructor(), SIMD.Bool32x4, "testConstructor");
equalSimd([true, true, true, true], m.testSplat(), SIMD.Bool32x4, "testSplat");
equalSimd([false, true, false, true], m.testLaneAccess(), SIMD.Bool32x4, "testLaneAccess");

// WScript.Echo((m.testConstructor().toString()));
// WScript.Echo((m.testSplat().toString()));
// WScript.Echo((m.testLaneAccess().toString()));
print("PASS");