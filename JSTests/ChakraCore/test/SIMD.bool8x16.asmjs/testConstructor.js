//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";

    var b16 = stdlib.SIMD.Bool8x16;
    var i16 = stdlib.SIMD.Int8x16;
    var i16lessThan = i16.lessThan;
    var b16check = b16.check;
    var b16splat = b16.splat;
    var b16and   = b16.and;
    var b16xor   = b16.xor;
    var b16extractLane = b16.extractLane;
    var b16replaceLane = b16.replaceLane;
 
    var globImportb16 = b16check(imports.g1);     
    var b16g1 = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    var loopCOUNT = 5;

    function testConstructor() {
        var a = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var b = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var i1 = i16(1, 0, 1, 8, 1, 0, 1, 8, 1, 0, 1, 8, 1, 0, 1, 8);
        var i2 = i16(12,13,0, 1, 12,13,0, 1, 12,13,0, 1, 12,13,0, 1);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b16(10*10,0,-1,loopIndex, 10*10,0,-1,loopIndex, 10*10,0,-1,loopIndex, 10*10,0,-1,loopIndex);
            b = i16lessThan(i1,i2);
            a = b16and(a, b);
        }
        return b16check(a);
    }
    
    function testSplat() {
        var a = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var b = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            a = b16splat(3);
            b = b16splat((3-3));
            a = b16xor(a, b);
        }
        return b16check(a);
    }
    
    function testLaneAccess() {
        var a = b16(5, 0, -10, 0, 10, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0);
        var result = b16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var m  = 0;
        var n  = 0;
        var o  = 0;
        var p  = 0;
        var m1 = 0;
        var n1 = 0;
        var o1 = 0;
        var p1 = 0;
        var m2 = 0;
        var n2 = 0;
        var o2 = 0;
        var p2 = 0;
        var m3 = 0;
        var n3 = 0;
        var o3 = 0;
        var p3 = 0;
        var loopIndex = 0;
        while ((loopIndex | 0) < (loopCOUNT | 0)) {
            loopIndex = (loopIndex + 1) | 0;
            m  = b16extractLane(a, 0)|0;
            n  = b16extractLane(a, 1)|0;
            o  = b16extractLane(a, 2)|0;
            p  = b16extractLane(a, 3)|0;
            m1 = b16extractLane(a, 4)|0;
            n1 = b16extractLane(a, 5)|0;
            o1 = b16extractLane(a, 6)|0;
            p1 = b16extractLane(a, 7)|0;
            m2 = b16extractLane(a, 8)|0;
            n2 = b16extractLane(a, 9)|0;
            o2 = b16extractLane(a, 10)|0;
            p2 = b16extractLane(a, 11)|0;
            m3 = b16extractLane(a, 12)|0;
            n3 = b16extractLane(a, 13)|0;
            o3 = b16extractLane(a, 14)|0;
            p3 = b16extractLane(a, 15)|0;
            result = b16replaceLane(result, 0, p3);
            result = b16replaceLane(result, 1, o3);
            result = b16replaceLane(result, 2, n3);
            result = b16replaceLane(result, 3, m3);
            result = b16replaceLane(result, 4, p2);
            result = b16replaceLane(result, 5, o2);
            result = b16replaceLane(result, 6, n2)
            result = b16replaceLane(result, 7, m2);
            result = b16replaceLane(result, 8, p1);
            result = b16replaceLane(result, 9, o1);
            result = b16replaceLane(result, 10, n1);
            result = b16replaceLane(result, 11, m1);
            result = b16replaceLane(result, 12, p);
            result = b16replaceLane(result, 13, o);
            result = b16replaceLane(result, 14, n);
            result = b16replaceLane(result, 15, m);
            result = b16(p, m, o, n, p1, m1, o1, n1, p2, m2, o2, n2, p3, m3, o3, n3);
        }
        return b16check(result);
    }

    //Validation will fail with the bug
    function retValueCoercionBug()
    {
        var ret1 = 0;
        var a = b16(1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8);
        ret1 = (b16extractLane(a, 0))|0;
    }

    return {testConstructor:testConstructor,
    testSplat:testSplat,
    testLaneAccess: testLaneAccess};
}

var m = asmModule(this, {g1:SIMD.Bool8x16(1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1)});

equalSimd([true, false, false, false, true, false, false, false, true, false, false, false, true, false, false, false], m.testConstructor(), SIMD.Bool8x16, "testConstructor");
equalSimd([true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], m.testSplat(), SIMD.Bool8x16, "testSplat");
equalSimd([false, true, true, false, false, true, true, false, false, true, true, false, false, true, true, false], m.testLaneAccess(), SIMD.Bool8x16, "testLaneAccess");

// WScript.Echo((m.testConstructor().toString()));
// WScript.Echo((m.testSplat().toString()));
// WScript.Echo((m.testLaneAccess().toString()));
print("PASS");