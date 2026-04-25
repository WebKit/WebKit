//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

function asmModule(stdlib, imports) {
    "use asm";


    var b4  = stdlib.SIMD.Bool32x4;
    var b8  = stdlib.SIMD.Bool16x8;
    var b16 = stdlib.SIMD.Bool8x16;
    
    var b4check  = b4.check;
    var b8check  = b8.check;
    var b16check = b16.check;
    
    var b4and  = b4.and;
    var b8and  = b8.and;
    var b16and = b16.and;
    
    var b4or = b4.or;
    var b8or = b8.or;
    var b16or= b16.or;
    
    var b4xor = b4.xor;
    var b8xor = b8.xor;
    var b16xor= b16.xor;
    
    var b4not = b4.not;
    var b8not  = b8.not;
    var b16not = b16.not;
    
    var b4allTrue = b4.allTrue;
    var b8allTrue  = b8.allTrue;
    var b16allTrue = b16.allTrue;
    
    var b4anyTrue = b4.anyTrue;
    var b8anyTrue  = b8.anyTrue;
    var b16anyTrue = b16.anyTrue;
    
    var g1 = b4(1,1,0,1);
    var g2 = b8(1,1,0,1,1,1,0,1);
    var g3 = b16(1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1);
    function and1(a)
    {
        a = a | 0;
        var b = b4(0,5,0,0);
        
        b = b4and(b,g1);
       
        return b4check(b);
        
    }
    function and2(a)
    {   a = a | 0;   var b = b8(0,5,0,0,0,5,0,0);
        b = b8and(b,g2); 
        return b8check(b);     
    } 
    function and3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        b = b16and(b,g3); 
        return b16check(b);     }
    
    function or1(a)
    {   a = a | 0;   var b = b4(0,5,0,0);
        b = b4or(b,g1); return b4check(b);     } 
            
    function or2(a)
    {   a = a | 0;   var b = b8(0,5,0,0,0,5,0,0);
        b = b8or(b,g2); return b8check(b);     } 
    function or3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        b = b16or(b,g3); 
        return b16check(b);     }
        
    function xor1(a)
    {   a = a | 0;   var b = b4(0,5,0,0);
        b = b4xor(b,g1); return b4check(b);     } 
            
    function xor2(a)
    {   a = a | 0;   var b = b8(0,5,0,0,0,5,0,0);
        b = b8xor(b,g2); return b8check(b);     } 
    function xor3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        b = b16xor(b,g3); 
        return b16check(b);     }
        
    function not1(a)
    {   a = a | 0;   var b = b4(0,5,0,0);
        b = b4not(b); return b4check(b);     } 
            
    function not2(a)
    {   a = a | 0;   var b = b8(0,5,0,0,0,5,0,0);
        b = b8not(b); return b8check(b);     } 
    function not3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        b = b16not(b); 
        return b16check(b);     }
        
    function allTrue1(a)
    {   a = a | 0;   
        var b1 = b4(0,5,0,0);
        var b2 = b4(-2, 2, 1, 3);
        var x = 0;
        
        x = ((b4allTrue(b1) | 0) + (b4allTrue(b2) | 0)) | 0;
        return x | 0;
    } 
            
    function allTrue2(a)
    {   a = a | 0;   
        var b1 = b8(0,5,0,0,1,5,0,1);
        var b2 = b8(1,5,3,1,0,5,-100,0);
        
        b1 = b8or(b1, b2);
        
        return b8allTrue(   b1) | 0;     
        
    } 
    
    function allTrue3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        return b16allTrue(b)| 0;    }    
        
    function anyTrue1(a)
    {   a = a | 0;   var b = b4(0,5,0,0);
        return b4anyTrue(b) | 0;     } 
            
    function anyTrue2(a)
    {   a = a | 0;   var b = b8(0,5,0,0,0,5,0,0);
        return b8anyTrue(b) | 0;     } 
    function anyTrue3(a)
    {   a = a | 0;   var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);
        return b16anyTrue(b) | 0;     }                           
       
        
    return {
        and1: and1,
        and2: and2,
        and3: and3,
        or1:  or1,
        or2:  or2,
        or3:  or3,
        xor1: xor1,
        xor2: xor2,
        xor3: xor3,
        not1: not1,
        not2: not2,
        not3: not3,
        allTrue1: allTrue1,
        allTrue2: allTrue2,
        allTrue3: allTrue3,
        anyTrue1: anyTrue1,
        anyTrue2: anyTrue2,
        anyTrue3: anyTrue3
    };
}

var m = asmModule(this, {g1:SIMD.Int8x16(0, 1, 2, 3, 4, 5, -6, -7, -8, 9, 10, 11, 12, 13, 14, 15)});
var b4 = SIMD.Bool32x4;
var b8 = SIMD.Bool16x8;
var b16 = SIMD.Bool8x16;

var ret1 = m.and1(1);
var ret2 = m.and2(1);
var ret3 = m.and3(1);
var ret4 = m.or1(1);
var ret5 = m.or2(1);
var ret6 = m.or3(1);
var ret7 = m.xor1(1);
var ret8 = m.xor2(1);
var ret9 = m.xor3(1);
var ret10 = m.not1(1);
var ret11 = m.not2(1);
var ret12 = m.not3(1);
var ret13 = m.allTrue1(1);
var ret14 = m.allTrue2(1);
var ret15 = m.allTrue3(1);
var ret16 = m.anyTrue1(1);
var ret17 = m.anyTrue2(1);
var ret18 = m.anyTrue3(1);

/*
printSimdBaseline(ret1, "SIMD.Bool32x4", "ret1", "and1");
printSimdBaseline(ret2, "SIMD.Bool16x8", "ret2", "and2");
printSimdBaseline(ret3, "SIMD.Bool8x16", "ret3", "and3");
printSimdBaseline(ret4, "SIMD.Bool32x4", "ret4", "or1");
printSimdBaseline(ret6, "SIMD.Bool8x16", "ret6", "or3");
printSimdBaseline(ret5, "SIMD.Bool16x8", "ret5", "or2");
printSimdBaseline(ret7, "SIMD.Bool32x4", "ret7", "xor1");
printSimdBaseline(ret8, "SIMD.Bool16x8", "ret8", "xor2");
printSimdBaseline(ret9, "SIMD.Bool8x16", "ret9", "xor3");
printSimdBaseline(ret10, "SIMD.Bool32x4", "ret10", "not1");
printSimdBaseline(ret11, "SIMD.Bool16x8", "ret11", "not2");
printSimdBaseline(ret12, "SIMD.Bool8x16", "ret12", "not3");
print(ret13);
print(ret14);
print(ret15);
print(ret16);
print(ret17);
print(ret18);
*/

equalSimd([false, true, false, false], ret1, SIMD.Bool32x4, "and1")
equalSimd([false, true, false, false, false, true, false, false], ret2, SIMD.Bool16x8, "and2")
equalSimd([false, true, false, false, false, true, false, false, false, true, false, false, false, true, false, false], ret3, SIMD.Bool8x16, "and3")
equalSimd([true, true, false, true], ret4, SIMD.Bool32x4, "or1")
equalSimd([true, true, false, true, true, true, false, true, true, true, false, true, true, true, false, true], ret6, SIMD.Bool8x16, "or3")
equalSimd([true, true, false, true, true, true, false, true], ret5, SIMD.Bool16x8, "or2")
equalSimd([true, false, false, true], ret7, SIMD.Bool32x4, "xor1")
equalSimd([true, false, false, true, true, false, false, true], ret8, SIMD.Bool16x8, "xor2")
equalSimd([true, false, false, true, true, false, false, true, true, false, false, true, true, false, false, true], ret9, SIMD.Bool8x16, "xor3")
equalSimd([true, false, true, true], ret10, SIMD.Bool32x4, "not1")
equalSimd([true, false, true, true, true, false, true, true], ret11, SIMD.Bool16x8, "not2")
equalSimd([true, false, true, true, true, false, true, true, true, false, true, true, true, false, true, true], ret12, SIMD.Bool8x16, "not3")
equal(ret13, 1);
equal(ret14, 1);
equal(ret15, 0);
equal(ret16, 1);
equal(ret17, 1);
equal(ret18, 1);
print("PASS");
