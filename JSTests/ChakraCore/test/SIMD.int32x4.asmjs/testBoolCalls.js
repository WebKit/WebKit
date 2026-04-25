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
    function func1(a)
    {
        a = b4check(a);
        var b = b4(0,5,0,0);
        b = b4check(ifunc1(a));
        return b4check(b);
    }
    function ifunc1(a)
    {   
        a = b4check(a); 
        return b4check(a);     
    } 
    
    function func2(a)
    {   a = b8check(a);
        var b = b8(0,5,0,0,0,5,0,0);
        
        b = b8check(ifunc2(a));
        
        return b8check(b);     
    } 
    
    function ifunc2(a)
    {
        a = b8check(a);
        return b8check(a);     
    } 
    
    function func3(a)
    {   a = b16check(a);
        var b = b16(0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0);

        b = b16check(ifunc3(a));
        return b16check(b);
    }
    
    function ifunc3(a)
    {   
        a = b16check(a);
        return b16check(a);     
    }
        

        
    return {
        func1: func1,
        func2: func2,
        func3: func3,
    };
}

var m = asmModule(this, {g1:SIMD.Int8x16(0, 1, 2, 3, 4, 5, -6, -7, -8, 9, 10, 11, 12, 13, 14, 15)});
var b4 = SIMD.Bool32x4;
var b8 = SIMD.Bool16x8;
var b16 = SIMD.Bool8x16;

var ret1 = m.func1(b4(0,1,2,3));
var ret2 = m.func2(b8(0,1,2,3,0,-1,0,-1));
var ret3 = m.func3(b16(0,1,2,3,0,-1,0,-1,0,1,2,3,0,-1,0,-1));

/*
printSimdBaseline(ret1, "SIMD.Bool32x4", "ret1", "func1");
printSimdBaseline(ret2, "SIMD.Bool16x8", "ret2", "func2");
printSimdBaseline(ret3, "SIMD.Bool8x16", "ret3", "func3");
*/

equalSimd([false, true, true, true], ret1, SIMD.Bool32x4, "func1")
equalSimd([false, true, true, true, false, true, false, true], ret2, SIMD.Bool16x8, "func2")
equalSimd([false, true, true, true, false, true, false, true, false, true, true, true, false, true, false, true], ret3, SIMD.Bool8x16, "func3")

print("PASS");
