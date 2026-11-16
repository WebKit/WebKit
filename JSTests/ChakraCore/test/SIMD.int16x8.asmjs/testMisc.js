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
    var i8extractLane       = i8.extractLane        ;  
    var i8swizzle           = i8.swizzle            ;
    var i8shuffle           = i8.shuffle            ;

    var i8splat             = i8.splat              ;
    var i8replaceLane       = i8.replaceLane        ;
    var i8and               = i8.and                ;
    var i8or                = i8.or                 ;
    var i8xor               = i8.xor                ;
    var i8not               = i8.not                ;
    var i8add               = i8.add                ;
    var i8sub               = i8.sub                ;
    var i8mul               = i8.mul                ;
    var i8neg               = i8.neg                ;
    var i8shiftLeftByScalar = i8.shiftLeftByScalar  ;
    var i8shiftRightByScalar= i8.shiftRightByScalar ;
    var i8addSaturate       = i8.addSaturate        ;
    var i8subSaturate      = i8.subSaturate       ;
    var i8load              = i8.load               ;
    var i8store             = i8.store              ;
    var i8fromFloat32x4Bits = i8.fromFloat32x4Bits  ;
    var i8fromInt32x4Bits   = i8.fromInt32x4Bits    ;
    var i8fromUint32x4Bits  = i8.fromUint32x4Bits   ;
    var i8fromUint16x8Bits  = i8.fromUint16x8Bits   ;
    var i8fromUint8x16Bits  = i8.fromUint8x16Bits   ;

    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;
    


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

    

    var i8g1 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432);
    var i8g2 = i8(0, -124, 6128, 10432, 216, -824, -3128, 10);
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
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var y = 0;
        var s = 0;
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        x = i8g2;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            y = i8extractLane(x, 0);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 1);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 2);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 3);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 4);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 5);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 6);
            s = ( s + y ) | 0;
            y = i8extractLane(x, 7);
            s = ( s + y ) | 0;
            
            loopIndex = (loopIndex + 1) | 0;
            
        }
        
        return s | 0;
    }
    
    function func2(a)
    {
        a = i8check(a);
        var x = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = i8swizzle(x, 1, 2, 3, 4, 0, 7, 7, 7);
            x = i8shuffle(a, x, 0, 1, 10, 11, 2, 3, 12, 13);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(x);
    }
    
    function func3(a)
    {
        a = i8check(a);
        var x = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i8splat(loopIndex);
            x = i8replaceLane(x, 0, 200);
            
            x = i8replaceLane(x, 7, loopIndex);
            y = x;
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(y);
    }
    
    
    function func4(a, b, c, d, e)
    {
        a = i8check(a);
        b = i8check(b);
        c = i8check(c);
        d = i8check(d);
        e = i8check(e);
        var x = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i8and(a, b);
            x = i8or(x, d);
            x = i8xor(x, e);
            x = i8not(x);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(x);
    }
    
    function func5(a, b, c, d, e)
    {
        a = i8check(a);
        b = i8check(b);
        c = i8check(c);
        d = i8check(d);
        e = i8check(e);
        var x = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var loopIndex = 0;
        var loopCOUNT = 3;
        
        
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = i8add(a, b);
            x = i8sub(x, d);
            x = i8mul(x, e);
            x = i8neg(x);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(x);
    }
        
    function func7()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = i8(1, 2, 3, 4, -1, -2, -3, -4);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }
    
    function func8()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = i8(1, 2, 3, 4, -1, -2, -3, -4);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var v_i4 = i4(-29,34,10,4);
        var v_f4 = f4(1.0, -200.0, 123.443, 3000.1);
        var v_u4 = u4(-29,0xffff,10,4);
        var v_u8 = u8(-29,0xffff,10,4, -104, -9999, 1, 22);
        var v_u16 = u16(-29,0xffff,10,4, -104, -9999, 1, 22, -29,0xffff,10,4, -104, -9999, 1, 22);
        
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            x = i8add(x, i8fromFloat32x4Bits(v_f4));
            x = i8add(x, i8fromInt32x4Bits(v_i4));
            x = i8add(x, i8fromUint32x4Bits(v_u4));
            x = i8add(x, i8fromUint16x8Bits(v_u8));
            x = i8add(x, i8fromUint8x16Bits(v_u16));
        }
        return i8check(x);
    }

    function bug1() //simd tmp reg reuse.
    {
        var a = i4(1,2,3,4);
        var x = i8(-1, -2, -3, -4,-1, -2, -3, -4);
        i8add(x,x);
        return i4check(a);
    }

    return {func1:func1, func2: func2, func3:func3, func4:func4,
            func5: func5, func7:func7, func8:func8, bug1:bug1};
}

var buffer = new ArrayBuffer(0x10000);
var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)/*, g3:SIMD.Float64x2(110.20, 58967.0, 14511.670, 191766.23431)*/}, buffer);



var v1 = SIMD.Int16x8(1, 2, 3, 4, 5, 6, 7, 8);
var v2 = SIMD.Int16x8(134, 211, -333, 422, -165, 999996, 0xffff, 0xf0f0);
var v3 = SIMD.Int16x8(0xcccc, -211, 189, 422, -165, -999996, 0xffff, 0xf0f0);
var ret1 = m.func1();
equal(38130, ret1);

var ret2 = m.func2(v1);
//printSimdBaseline(ret2, "SIMD.Int16x8", "ret2", "func2");
equalSimd([1, 2, 3, 3, 3, 4, 1, 0], ret2, SIMD.Int16x8, "func2");

var ret3 = m.func3(v1);
//printSimdBaseline(ret3, "SIMD.Int16x8", "ret3", "func3");
equalSimd([200, 2, 2, 2, 2, 2, 2, 2], ret3, SIMD.Int16x8, "func3");

var ret4 = m.func4(v1, v2, v3, v1, v2);
//printSimdBaseline(ret4, "SIMD.Int16x8", "ret4", "func4");
equalSimd([-136, -210, 335, -419, 161, -16955, 7, 3847], ret4, SIMD.Int16x8, "func4");

var ret5 = m.func5(v1, v2, v3, v1, v2);
//printSimdBaseline(ret5, "SIMD.Int16x8", "ret5", "func5");
equalSimd([-17956, 21015, 20183, 18524, -27225, 496, -1, 7936], ret5, SIMD.Int16x8, "func5");

var ret7 = m.func7(v1, v2, v3, v1, v2);
//printSimdBaseline(ret7, "SIMD.Int16x8", "ret7", "func7");
equalSimd([10, 20, 30, 40, -10, -20, -30, -40], ret7, SIMD.Int16x8, "func7");

var ret8 = m.func8(v1, v2, v3, v1, v2);
//printSimdBaseline(ret8, "SIMD.Int16x8", "ret8", "func8");
equalSimd([-1159, -23736, 29089, 32006, -10305, 16232, -32693, -28368], ret8, SIMD.Int16x8, "func8")

ret8 = m.bug1();
//printSimdBaseline(ret8, "SIMD.Int16x8", "ret8", "func8");
equalSimd([1,2,3,4], ret8, SIMD.Int32x4, "bug1")


print("PASS");


