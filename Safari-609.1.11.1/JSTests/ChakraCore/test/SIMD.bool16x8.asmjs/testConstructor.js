//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b8 = stdlib.SIMD.Bool16x8;
    var i8 = stdlib.SIMD.Int16x8;
    var i8lessThan = i8.lessThan;
    var b8check = b8.check;
    var b8splat = b8.splat;
    var b8and   = b8.and;
    var b8xor   = b8.xor;
    var b8extractLane = b8.extractLane;
    var b8replaceLane = b8.replaceLane;
 
    var globImportb8 = b8check(imports.g1);     
    var b8g1 = b8(0, 0, 0, 0, 0, 0, 0, 0);
    var loopCOUNT = 5;

    function testConstructor() {
        var a = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var b = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var i1 = i8(1, 0, 1, 8, 1, 0, 1, 8);
        var i2 = i8(12,13,0, 1, 12,13,0, 1);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b8(10*10,0,-1,loopIndex, 10*10,0,-1,loopIndex);
            b = i8lessThan(i1,i2);
            a = b8and(a, b);
        }
        return b8check(a);
    }
    
    function testSplat() {
        var a = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var b = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b8splat(3);
            b = b8splat((3-3));
            a = b8xor(a, b);
        }
        return b8check(a);
    }
    
    function testLaneAccess() {
        var a = b8(5, 0, -50, 0, 10, 0, 1, 0);
        var result = b8(0, 0, 0, 0, 0, 0, 0, 0);
        var m  = 0;
        var n  = 0;
        var o  = 0;
        var p  = 0;
        var m1 = 0;
        var n1 = 0;
        var o1 = 0;
        var p1 = 0;
        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            m  = b8extractLane(a, 0)|0;
            n  = b8extractLane(a, 1)|0;
            o  = b8extractLane(a, 2)|0;
            p  = b8extractLane(a, 3)|0;
            m1 = b8extractLane(a, 4)|0;
            n1 = b8extractLane(a, 5)|0;
            o1 = b8extractLane(a, 6)|0;
            p1 = b8extractLane(a, 7)|0;
            result = b8replaceLane(a, 0, p1);
            result = b8replaceLane(result, 1, o1);
            result = b8replaceLane(result, 2, n1);
            result = b8replaceLane(result, 3, m1);
            result = b8replaceLane(result, 4, p);
            result = b8replaceLane(result, 5, o);
            result = b8replaceLane(result, 6, n);
            result = b8replaceLane(result, 7, m);
        }
        return b8check(result);
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret1 = 0;
        var a = b8(1,2,3,4,5,6,7,8);
        ret1 = (b8extractLane(a, 0))|0;
    }
    return {testConstructor:testConstructor,
    testSplat:testSplat,
    testLaneAccess: testLaneAccess};
}

var m = asmModule(this, {g1:SIMD.Bool16x8(1, 0, 1, 1, 1, 0, 1, 1)});

equalSimd([true, false, false, false, true, false, false, false], m.testConstructor(), SIMD.Bool16x8, "testConstructor");
equalSimd([true, true, true, true, true, true, true, true], m.testSplat(), SIMD.Bool16x8, "testSplat");
equalSimd([false, true, false, true, false, true, false, true], m.testLaneAccess(), SIMD.Bool16x8, "testLaneAccess");

// WScript.Echo((m.testConstructor().toString()));
// WScript.Echo((m.testSplat().toString()));
// WScript.Echo((m.testLaneAccess().toString()));
print("PASS");