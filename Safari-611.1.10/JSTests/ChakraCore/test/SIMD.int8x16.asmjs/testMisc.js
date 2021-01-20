//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports,buffer) {
    "use asm";
    
    
var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;
    //var i4abs = i4.abs;
    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
    //var i4swizzle = i4.swizzle;
    //var i4shuffle = i4.shuffle;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;
    var i4shiftLeftByScalar = i4.shiftLeftByScalar;
    var i4shiftRightByScalar = i4.shiftRightByScalar;
    
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;    
    
    
    
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16extractLane       = i16.extractLane        ;  
    var i16swizzle           = i16.swizzle            ;
    var i16shuffle           = i16.shuffle            ;

    var i16splat             = i16.splat              ;
    var i16replaceLane       = i16.replaceLane        ;
    var i16and               = i16.and                ;
    var i16or                = i16.or                 ;
    var i16xor               = i16.xor                ;
    var i16not               = i16.not                ;
    var i16add               = i16.add                ;
    var i16sub               = i16.sub                ;
    var i16mul               = i16.mul                ;
    var i16shiftLeftByScalar = i16.shiftLeftByScalar  ;
    var i16shiftRightByScalar= i16.shiftRightByScalar ;
    var i16addSaturate       = i16.addSaturate        ;
    var  i16subSaturate      = i16.subSaturate       ;
    var i16load              = i16.load               ;
    var i16store             = i16.store              ;
    var i16fromFloat32x4Bits = i16.fromFloat32x4Bits  ;
    var i16fromInt32x4Bits   = i16.fromInt32x4Bits    ;
    var i16fromInt16x8Bits  = i16.fromInt16x8Bits   ;
    var i16fromUint32x4Bits  = i16.fromUint32x4Bits   ;
    var i16fromUint16x8Bits  = i16.fromUint16x8Bits   ;

    
   
    


    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    var f4splat = f4.splat;
    
    var f4fromInt32x4 = f4.fromInt32x4;
    var f4fromInt32x4Bits = f4.fromInt32x4Bits;
    var f4abs = f4.abs;
    var f4neg = f4.neg;
    var f4add = f4.add;
    var f4sub = f4.sub;
    var f4mul = f4.mul;
    var f4div = f4.div;
    var f4min = f4.min;
    var f4max = f4.max;


    var f4sqrt = f4.sqrt;
    //var f4swizzle = f4.swizzle;
    //var f4shuffle = f4.shuffle;
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;

    

    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    

    var f4g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var f4g2 = f4(1194580.33,-11201.5,63236.93,334.8);          // global var initialized

    var i4g1 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var i4g2 = i4(353216, -492529, -1128, 1085);          // global var initialized

    

    var i16g1 = i16(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432, 0, -124, 6128, 10432, 216, -824, -3128, 10);
    var i16g2 = i16(0, -124, 6128, 10432, 216, -824, -3128, 10, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432, 0, -124);
    var gval = 1234;
    var gval2 = 1234.0;


    var Int8Heap = new stdlib.Int8Array (buffer);    
    var Uint8Heap = new stdlib.Uint8Array (buffer);    

    var Int16Heap = new stdlib.Int16Array(buffer);
    var Uint16Heap = new stdlib.Uint16Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var Uint32Heap = new stdlib.Uint32Array(buffer);
    var Float32Heap = new stdlib.Float32Array(buffer);
    
    var loopCOUNT = 3;

    function func1()
    {
        var x = i16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8);;
        var y = 0;
        var s = 0;
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        x = i16g2;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            y = i16extractLane(x, 0);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 1);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 2);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 3);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 4);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 5);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 6);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 7);
            s = ( s + y ) | 0;
            
            y = i16extractLane(x, 8);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 9);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 10);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 11);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 12);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 13);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 14);
            s = ( s + y ) | 0;
            y = i16extractLane(x, 15);
            s = ( s + y ) | 0;
            
            loopIndex = (loopIndex + 1) | 0;
            
        }
        
        return s | 0;
    }
    
    function func2(a)
    {
        a = i16check(a);
        var x = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i16swizzle(x, 1, 2, 3, 4, 0, 7, 7, 7, 11, 13, 12, 14, 10, 15, 15, 15);
            x = i16shuffle(a, x, 0, 1, 10, 11, 2, 3, 12, 13, 20, 21, 31, 3, 22, 0, 18, 31);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i16check(x);
    }
    
    function func3(a)
    {
        a = i16check(a);
        var x = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i16splat(loopIndex);
            x = i16replaceLane(x, 0, 200);
            x = i16replaceLane(y, 9, loopIndex);
            y = x;
            loopIndex = (loopIndex + 1) | 0;
        }
        y = i16add(y, i16(0,1,1,1,loopIndex, loopIndex, loopIndex, loopIndex,1,1,1,1,0,0,0,0));
        return i16check(y);
    }
    
    
    function func4(a, b, c, d, e)
    {
        a = i16check(a);
        b = i16check(b);
        c = i16check(c);
        d = i16check(d);
        e = i16check(e);
        var x = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i16and(a, b);
            x = i16or(x, d);
            x = i16xor(x, e);
            x = i16not(x);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i16check(x);
    }
    
    function func5(a, b, c, d, e)
    {
        a = i16check(a);
        b = i16check(b);
        c = i16check(c);
        d = i16check(d);
        e = i16check(e);
        var x = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i16add(a, b);
            x = i16sub(x, d);
            x = i16mul(x, e);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i16check(x);
    }
    
    
    function func7()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = i16(1, 2, 3,0xff, -1, -2, -3, -4, 16, 24, 32, 41, -18, -27, -63, 0xff);
        var y = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var t = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i16load(Float32Heap, index >> 2);
            y = i16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(y);
    }
    
    function func8()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = i16(1, 2, 3,0xff, -1, -2, -3, -4, 16, 24, 32, 41, -18, -27, -63, 0xff);
        var y = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var t = i16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var v_i4 = i4(-29,34,10,4);
        var v_f4 = f4(1.0, -200.0, 123.443, 3000.1);
        var v_u4 = u4(-29,0xffff,10,4);
        var v_i8 = i8(-29,0xffff,10,4, -104, -9999, 1, 22);
        var v_u8 = u8(-29,0xffff,10,4, -104, -9999, 1, 22);
        
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = i16add(x, i16fromFloat32x4Bits(v_f4));
            x = i16add(x, i16fromInt32x4Bits(v_i4));
            x = i16add(x, i16fromInt16x8Bits(v_i8));
            x = i16add(x, i16fromUint32x4Bits(v_u4));
            x = i16add(x, i16fromUint16x8Bits(v_u8));
        }

        return i16check(x);
    }

    function bug1() //simd tmp reg reuse.
    {
        var a = i4(1,2,3,4);
        var x = i16(-1, -2, -3, -4,-1, -2, -3, -4, -1, -2, -3, -4,-1, -2, -3, -4);
        i16add(x,x);
        return i4check(a);
    }

    return {func1:func1, func2: func2, func3:func3, func4:func4,
            func5: func5, func7:func7, func8:func8, bug1:bug1};
}

var buffer = new ArrayBuffer(0x10000);
var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)}, buffer);



var v1 = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
var v2 = SIMD.Int8x16(134, 211, -333, 422, -165, 999996, 0xffff, 0xf0f0, 134, 211, -333, 422, -165, 999996, 0xffff, 0xf0f0);
var v3 = SIMD.Int8x16(0xcccc, -211, 189, 422, -165, -999996, 0xffff, 0xf0f0, 0xcccc, -211, 189, 422, -165, -999996, 0xffff, 0xf0f0);
var ret1 = m.func1();
// print (ret1);
var ret2 = m.func2(v1);
// printSimdBaseline(ret2, "SIMD.Int8x16", "ret2", "func2");


var ret3 = m.func3(v1);
// printSimdBaseline(ret3, "SIMD.Int8x16", "ret3", "func3");


var ret4 = m.func4(v1, v2, v3, v1, v2);
// printSimdBaseline(ret4, "SIMD.Int8x16", "ret4", "func4");


var ret5 = m.func5(v1, v2, v3, v1, v2);
// printSimdBaseline(ret5, "SIMD.Int8x16", "ret5", "func5");


var ret7 = m.func7(v1, v2, v3, v1, v2);
// printSimdBaseline(ret7, "SIMD.Int8x16", "ret7", "func7");


var ret8 = m.func8(v1, v2, v3, v1, v2);
// printSimdBaseline(ret8, "SIMD.Int8x16", "ret8", "func8");

var ret9 = m.bug1();
// printSimdBaseline(ret9, "SIMD.Int32x4", "ret9", "bug1");

equal(-1410, ret1);
equalSimd([1, 2, 3, 4, 3, 4, 5, 6, 1, 6, 0, 4, 6, 1, 4, 0], ret2, SIMD.Int8x16, "func2")
equalSimd([0, 1, 1, 1, 3, 3, 3, 3, 1, 3, 1, 1, 0, 0, 0, 0], ret3, SIMD.Int8x16, "func3")
equalSimd([120, 46, 79, 93, -95, -59, 7, 7, 120, 46, 79, 93, -95, -59, 7, 7], ret4, SIMD.Int8x16, "func4")
equalSimd([36, -23, 41, -92, 89, 16, 1, 0, 36, -23, 41, -92, 89, 16, 1, 0], ret5, SIMD.Int8x16, "func5")
equalSimd([10, 20, 30, -10, -10, -20, -30, -40, -96, -16, 64, -102, 76, -14, -118, -10], ret7, SIMD.Int8x16, "func7")
equalSimd([121, -38, -37, 77, 17, -12, 29, -102, -30, -40, -112, -99, 86, -17, -57, -79], ret8, SIMD.Int8x16, "func8")
equalSimd([1, 2, 3, 4], ret9, SIMD.Int32x4, "bug1")

print("PASS");


