//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function InitBuffer1(buffer) {
    HEAP32 = new Int32Array(buffer);
    HEAP32[0] = 13654
    HEAP32[1] = 23
    HEAP32[2] = 13654
    HEAP32[3] = 13654
    HEAP32[4] = 423
    HEAP32[5] = 4
    HEAP32[6] = 5
    HEAP32[7] = 3
    HEAP32[8] = 145764
    HEAP32[9] = 53
}

function InitBuffer2(buffer) {
    HEAP32 = new Int32Array(buffer);
    for(var i = 0; i < 999999 - 1; i++) {
        HEAP32[i] = i + 1;
    }
}

function asmModule(stdlib, imports, buffer) {
    "use asm";

    var log = stdlib.Math.log;

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
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4equal = i4.equal;
    var i4splat = i4.splat;
    var i4swizzle = i4.swizzle;
    var i4extractLane = i4.extractLane;

    var HEAP32 = new stdlib.Int32Array(buffer);
    var BLOCK_SIZE = 4;
    var i = 0;

    function linearSearch(value, length) {
        value = value | 0;
        length = length|0;
        var i4Value = i4(0, 0, 0, 0);
        var i4Heap = i4(0, 0, 0, 0);
        var i4Result = i4(0, 0, 0, 0);
        var b4Result = b4(0, 0, 0, 0);
        var i4Flipped = i4(0, 0, 0, 0);
        var iVal = 0, loc = 0;
        
        i4Value = i4splat(value | 0);
        for(i = 0; (i | 0) < (length | 0); i = (i + BLOCK_SIZE) | 0) {
                i4Heap = i4((HEAP32[(i << 2) >> 2] | 0), (HEAP32[((i + 1) << 2) >> 2] | 0), (HEAP32[((i + 2) << 2) >> 2] | 0), (HEAP32[((i + 3) << 2) >> 2] | 0));
                b4Result = i4equal(i4Heap, i4Value);

                if((b4anyTrue(b4Result) | 0 )!= 0) {
                    i4Flipped = i4swizzle(i4Heap, 3, 2, 1, 0);
                    iVal = i4extractLane(i4Flipped, 0);
                    if ( (iVal | 0) == (value | 0))
                    { loc = 0;}
                
                    iVal = i4extractLane(i4Flipped, 1);
                    if ( (iVal | 0) == (value | 0))
                    { loc = 1;}
                
                    iVal = i4extractLane(i4Flipped, 2);
                    if ( (iVal | 0) == (value | 0))
                    { loc = 2;}
                
                    iVal = i4extractLane(i4Flipped, 3);
                    if ( (iVal | 0) == (value | 0))
                    { loc = 3;}
                    
                    return (i + BLOCK_SIZE - loc - 1)|0
                }
        }
        return -1;
    }

    return {linearSearch:linearSearch};
}

var buffer = new ArrayBuffer(16 * 1024 * 1024);
var m = asmModule(this, null, buffer);

InitBuffer1(buffer);
WScript.Echo("List 1");
WScript.Echo(m.linearSearch(13654, 10));
WScript.Echo(m.linearSearch(23, 10));
WScript.Echo(m.linearSearch(145764, 10));
WScript.Echo(m.linearSearch(53, 10));
WScript.Echo(m.linearSearch(-53, 10));

InitBuffer2(buffer);
WScript.Echo("List 2");
WScript.Echo(m.linearSearch(13654, 999999));
WScript.Echo(m.linearSearch(23, 999999));
WScript.Echo(m.linearSearch(145764, 999999));
WScript.Echo(m.linearSearch(-53, 999999));
