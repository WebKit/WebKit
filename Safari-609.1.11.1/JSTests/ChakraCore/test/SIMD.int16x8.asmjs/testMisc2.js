//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports, buffer) {
    "use asm";
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4fromU4Bits = i4.fromUint32x4Bits;

    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8fromU8Bits = i8.fromUint16x8Bits;
    var i8extractLane = i8.extractLane;
    var i8replaceLane = i8.replaceLane;
    var i8swizzle = i8.swizzle;
    var i8shuffle = i8.shuffle;
    var i8load = i8.load;
    var i8store = i8.store;

    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fromU16Bits = i16.fromUint8x16Bits;
    var u16 = stdlib.SIMD.Uint8x16;
    var u16check = u16.check;
    var u16extractLane = u16.extractLane;
    var u16replaceLane = u16.replaceLane;
    var u16swizzle = u16.swizzle;
    var u16shuffle = u16.shuffle;
    var u16load = u16.load;
    var u16store = u16.store;
        
    var u8 = stdlib.SIMD.Uint16x8;
    var u8check = u8.check;
    var u8extractLane = u8.extractLane;
    var u8replaceLane = u8.replaceLane;
    var u8swizzle = u8.swizzle;
    var u8shuffle = u8.shuffle;
    var u8load = u8.load;
    var u8store = u8.store;
    
    var u4 = stdlib.SIMD.Uint32x4;
    var u4check = u4.check;
    var u4extractLane = u4.extractLane;
    var u4replaceLane = u4.replaceLane;
    var u4swizzle = u4.swizzle;
    var u4shuffle = u4.shuffle;
    var u4load    = u4.load;    
    var u4load1   = u4.load1;
    var u4load2   = u4.load2;
    var u4load3   = u4.load3;
    var u4store    = u4.store;    
    var u4store1   = u4.store1;
    var u4store2   = u4.store2;
    var u4store3   = u4.store3;
    
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
    var globImportI8 = i8check(imports.g2);       // global var import
    
    
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i8(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    
    var g4 = u8(1065353216, -1073741824, -1077936128, 1082130432, 1065353216, -1073741824, -1077936128, 1082130432);         
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

    function func1(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);;

        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = g2;
            x = i8(loopIndex, ((-1 * loopIndex) | 0) - 1, loopIndex + 2, loopIndex + 3, loopIndex + 4, ((-1 * loopIndex) | 0) - 5, loopIndex + 6, loopIndex + 7);
            loopIndex = (loopIndex + 1) | 0;
        }
        g2 = x;
        return i8check(x);
    }
    
    function func2(a, b, c, d)
    {
        a = i8check(a);
        b = i8check(b);
        c = i8check(c);
        d = i8check(d);
        var x = i8(-1, -2, -3, -4, -5, -6, -7, -8);
        var y = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var z = 9;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            x = i8check(func1(a, b));
            y = i8check(func1(c, d));
            x = i8(loopIndex, loopCOUNT + 1, z + 2, loopIndex + 3, z + 4, loopIndex + 5, z + 6, loopIndex + 7);    
            z = (z + ((loopIndex * 2) | 0)) |0 ;
        }
        g2 = x;
        
        return i8check(x);
    }

    
    function func3(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u8(-1, -2, -3, -4, -5, -6, -7, -8);;

        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = u8(loopIndex, ((-1 * loopIndex) | 0) - 1, loopIndex + 2, loopIndex + 3, -1, loopIndex + 5, ((-1 * loopIndex) | 0) - 100, loopIndex);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fromU8Bits(x));
    }
    
    
    function func4(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;

        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = u4(loopIndex, ((-1 * loopIndex) | 0) - 1, loopIndex + 2, -1);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return i4check(i4fromU4Bits(x));
    }
    
    function func5(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -17);;

        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            x = u16(loopIndex, ((-1 * loopIndex) | 0) - 1, loopIndex + 2, loopIndex + 3, -1, loopIndex + 5, ((-1 * loopIndex) | 0) - 100, loopIndex ,
                    loopIndex + 1, ((-2 * loopIndex) | 0) - 1, loopIndex + 3, loopIndex + 4, -100, loopIndex + 7, ((-100 * loopIndex) | 0) - 100, loopIndex);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fromU16Bits(x));
    }
    
    
    function func6(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -17);;
        var y = 0;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            
            y = u16extractLane(x, 0);
            y = (y + u16extractLane(x, 14)) | 0;
            y = (y + u16extractLane(x, 13)) | 0;
            y = (y + u16extractLane(x, 12)) | 0;
            y = (y + u16extractLane(x, 11)) | 0;
            y = (y + u16extractLane(x, 10)) | 0;
            y = (y + u16extractLane(x, 9)) | 0;
            y = (y + u16extractLane(x, 8)) | 0;
            y = (y + u16extractLane(x, 7)) | 0;
            y = (y + u16extractLane(x, 6)) | 0;
            y = (y + u16extractLane(x, 5)) | 0;
            y = (y + u16extractLane(x, 4)) | 0;
            y = (y + u16extractLane(x, 3)) | 0;
            y = (y + u16extractLane(x, 2)) | 0;
            y = (y + u16extractLane(x, 1)) | 0;
            y = (y + u16extractLane(x, 0)) | 0;
            
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return y | 0;
    }
    
    function func7(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var y = 0;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            //y = u8extractLane(x, 1);
            y = (u8extractLane(x, 0) + i8extractLane(z, 7)) | 0;
            y = ( ((y + u8extractLane(x, 1)) |0) + i8extractLane(z, 6)) | 0;
            y = ( ((y + u8extractLane(x, 2)) |0) + i8extractLane(z, 5)) | 0;
            y = ( ((y + u8extractLane(x, 3)) |0) + i8extractLane(z, 4)) | 0;
            y = ( ((y + u8extractLane(x, 4)) |0) + i8extractLane(z, 3)) | 0;
            y = ( ((y + u8extractLane(x, 5)) |0) + i8extractLane(z, 2)) | 0;
            y = ( ((y + u8extractLane(x, 6)) |0) + i8extractLane(z, 1)) | 0;
            y = ( ((y + u8extractLane(x, 7)) |0) + i8extractLane(z, 0)) | 0;
            
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return y | 0;
    }
    
    
    function func8(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            u4replaceLane(x, 2, (loopIndex * 100) | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i4check(i4fromU4Bits(x));
    }
    
    function func9(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            i8replaceLane(y, 7, (loopIndex * 100) | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(y);
    }
    
    function func10(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            u8replaceLane(z, 6, (loopIndex * 100) | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(i8fromU8Bits(z));
    }
    
    function func11(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            u16replaceLane(w, 15, (loopIndex * 100) | 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i16check(i16fromU16Bits(w));
    }
    
    function func12(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = u4swizzle(x, 3, 2, 1, 0);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i4check(i4fromU4Bits(x));
    }
    
    
    function func13(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            y = i8swizzle(y, 3, 2, 1, 0, 7, 6, 5, 4);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(y);
    }
    
    function func14(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            z = u8swizzle(z, 7, 6, 5, 4, 3, 2, 1, 0);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fromU8Bits(z));
    }
    
    function func15(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            
            w = u16swizzle(w, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fromU16Bits(w));
    }
    
    function func16(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var x2 = u4(-100, -200, -300, -400);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            x = u4shuffle(x, x2, 7, 6, 3, 1);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i4check(i4fromU4Bits(x));
    }
    
    
    function func17(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var y2 = i8(-100, -200, -300, -400, -500, -600, -700, -800);
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            y = i8shuffle(y, y2, 15, 14, 13, 12, 7, 6, 5, 4);
            loopIndex = (loopIndex + 1) | 0;
        }
        
        return i8check(y);
    }
    
    function func18(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z2 = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8,);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            z = u8shuffle(z, z2, 15, 14, 13, 12, 7, 6, 5, 4);
            loopIndex = (loopIndex + 1) | 0;
        }
        return i8check(i8fromU8Bits(z));
    }
    
    function func19(a, b)
    {
        a = i8check(a);
        b = i8check(b);
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var z = u8(-1, -2, -3, -4, -5, -6, -7, -8);;
        var w = u16(-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3, -4, -5, -6, -7, -8);;
        var w2 = u16(-100, -200, -300, -400, -500, -600, -700, -800, -100, -200, -300, -400, -500, -600, -700, -800);;
        var loopIndex = 0;
        var loopCOUNT = 1;
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            
            w = u16shuffle(w, w2, 7, 6, 5, 4, 3, 2, 1, 0, 31, 30, 29, 28, 11, 10, 9, 8);
            
            loopIndex = (loopIndex + 1) | 0;
        }
        return i16check(i16fromU16Bits(w));
    }
    
    function func20()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            u4store(Uint32Heap, index >> 2, x);
            
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            
            x = u4load(Float32Heap, index >> 2);
            
            index = (index + 16 ) | 0;
        }
        return i4check(i4fromU4Bits(x));
    }
    
    function func21()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, y);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            y = i8load(Float32Heap, index >> 2);

            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }
    
    function func22()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            
            u8store(Uint32Heap, index >> 2, z);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            z = u8load(Float32Heap, index >> 2);
            index = (index + 16 ) | 0;
        }
        return i8check(i8fromU8Bits(z));
    }
    
    function func23()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            u16store(Uint32Heap, index >> 2, w);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            w = u16load(Float32Heap, index >> 2);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fromU16Bits(w));
    }
    
    function func24()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            u4store1(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            
            x = u4load1(Float32Heap, index >> 2);
            index = (index + 16 ) | 0;
        }
        return i4check(i4fromU4Bits(x));
    }
    
    function func25()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            u4store2(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            
            x = u4load2(Float32Heap, index >> 2);
            index = (index + 16 ) | 0;
        }
        return i4check(i4fromU4Bits(x));
    }
    
    function func26()
    {
        var loopIndex = 0;
        var loopCOUNT = 3;
        var index = 100;
        var size = 10;
        var x = u4(-1, -2, -3, -4);;
        var y = i8(-10, -20, -30, -40, -50, -60, -70, -80);;
        var z = u8(-100, -200, -300, -400, -500, -600, -700, -800);;
        var w = u16(-1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000);;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            u4store3(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            
            x = u4load3(Float32Heap, index >> 2);
            index = (index + 16 ) | 0;
        }
        return i4check(i4fromU4Bits(x));
    }
    
    
    function value()
    {
        var ret = 1.0;
        var i = 1.0;


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            ret = ret + i;

            loopIndex = (loopIndex + 1) | 0;
        }

        return +ret;
    }
    
    return {func1:func1, func2:func2, func3: func3, func4: func4, 
            func5: func5, func6: func6, func7 : func7, func8: func8,
            func9: func9, func10: func10, func11: func11, func12: func12,
            func13: func13, func14:func14, func15:func15, func16: func16,
            func17: func17, func18: func18, func19: func19, func20 : func20,
            func21 : func21, func22: func22, func23: func23, func24: func24,
            func25: func25, func26: func26
            };
}

var buffer = new ArrayBuffer(0x10000);
var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int16x8(-1065353216, -1073741824,-1077936128, -1082130432, -1065353216, -1073741824,-1077936128, -1082130432)}, buffer);

var s1 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s2 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s3 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s4 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s5 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s6 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s7 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
var s8 = SIMD.Int16x8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);

var ret1 = m.func1(s1, s2);
var ret1 = m.func1(s1, s2);
var ret1 = m.func1(s1, s2);

var ret2 = m.func2(s1, s2, s3, s4);
var ret2 = m.func2(s1, s2, s3, s4);
var ret2 = m.func2(s1, s2, s3, s4);

var ret3 = m.func3(s1, s2, s3, s4);
var ret3 = m.func3(s1, s2, s3, s4);
var ret3 = m.func3(s1, s2, s3, s4);

var ret4 = m.func4(s1, s2);
var ret4 = m.func4(s1, s2);
var ret4 = m.func4(s1, s2);

var ret5 = m.func5(s1, s2);
var ret5 = m.func5(s1, s2);
var ret5 = m.func5(s1, s2);

var ret6 = m.func6(s1, s2);
var ret6 = m.func6(s1, s2);
var ret6 = m.func6(s1, s2);

var ret7 = m.func7(s1, s2);
var ret7 = m.func7(s1, s2);
var ret7 = m.func7(s1, s2);

var ret8 = m.func8(s1, s2);
var ret8 = m.func8(s1, s2);
var ret8 = m.func8(s1, s2);

var ret9 = m.func9(s1, s2);
var ret9 = m.func9(s1, s2);
var ret9 = m.func9(s1, s2);

var ret10 = m.func10(s1, s2);
var ret10 = m.func10(s1, s2);
var ret10 = m.func10(s1, s2);

var ret11 = m.func11(s1, s2);
var ret11 = m.func11(s1, s2);
var ret11 = m.func11(s1, s2);



var ret12 = m.func12(s1, s2);
var ret12 = m.func12(s1, s2);
var ret12 = m.func12(s1, s2);


var ret13 = m.func13(s1, s2);
var ret13 = m.func13(s1, s2);
var ret13 = m.func13(s1, s2);


var ret14 = m.func14(s1, s2);
var ret14 = m.func14(s1, s2);
var ret14 = m.func14(s1, s2);


var ret15 = m.func15(s1, s2);
var ret15 = m.func15(s1, s2);
var ret15 = m.func15(s1, s2);


var ret16 = m.func16(s1, s2);
var ret16 = m.func16(s1, s2);
var ret16 = m.func16(s1, s2);


var ret17 = m.func17(s1, s2);
var ret17 = m.func17(s1, s2);
var ret17 = m.func17(s1, s2);


var ret18 = m.func18(s1, s2);
var ret18 = m.func18(s1, s2);
var ret18 = m.func18(s1, s2);


var ret19 = m.func19(s1, s2);
var ret19 = m.func19(s1, s2);
var ret19 = m.func19(s1, s2);

var ret20 = m.func20(s1, s2);
var ret20 = m.func20(s1, s2);
var ret20 = m.func20(s1, s2);

// loads/stores
var Int32Heap = new Int32Array(buffer);
ret20 = SIMD.Uint32x4.fromInt32x4Bits(ret20);
//printSimdBaseline(ret20, "SIMD.Uint32x4", "ret20", "func20");
equalSimd([4294967295, 4294967294, 4294967293, 4294967292], ret20 , SIMD.Uint32x4, "func20")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func20");
    equalSimd([-1, -2, -3, -4], s, SIMD.Int32x4, "func20")
}


var ret21 = m.func21(s1, s2);
var ret21 = m.func21(s1, s2);
var ret21 = m.func21(s1, s2);

//printSimdBaseline(ret21, "SIMD.Int16x8", "ret21", "func21");
equalSimd([-10, -20, -30, -40, -50, -60, -70, -80], ret21, SIMD.Int16x8, "func21")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func21");
    equalSimd([-1245194, -2555934, -3866674, -5177414], s, SIMD.Int32x4, "func21")
}

var ret22 = m.func22(s1, s2);
var ret22 = m.func22(s1, s2);
var ret22 = m.func22(s1, s2);
ret22 = SIMD.Uint16x8.fromInt16x8Bits(ret22);
//printSimdBaseline(ret22, "SIMD.Uint16x8", "ret22", "func22");
equalSimd([65436, 65336, 65236, 65136, 65036, 64936, 64836, 64736], ret22, SIMD.Uint16x8, "func22")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func22");
    equalSimd([-13041764, -26149164, -39256564, -52363964], s, SIMD.Int32x4, "func22")
}

var ret23 = m.func23(s1, s2);
var ret23 = m.func23(s1, s2);
var ret23 = m.func23(s1, s2);

//printSimdBaseline(ret23, "SIMD.Uint8x16", "ret23", "func23");

ret23 = SIMD.Uint8x16.fromInt8x16Bits(ret23);
equalSimd([24, 48, 72, 96, 120, 144, 168, 192, 24, 48, 72, 96, 120, 144, 168, 192], ret23, SIMD.Uint8x16, "func23")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func23");
    equalSimd([1615343640, -1062694792, 1615343640, -1062694792], s, SIMD.Int32x4, "func23")
}

var ret24 = m.func24(s1, s2);
var ret24 = m.func24(s1, s2);
var ret24 = m.func24(s1, s2);
ret24 = SIMD.Uint32x4.fromInt32x4Bits(ret24);
//printSimdBaseline(ret24, "SIMD.Uint32x4", "ret24", "func24");
equalSimd([4294967295, 0, 0, 0], ret24, SIMD.Uint32x4, "func24")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func24");
    equalSimd([-1, -1062694792, 1615343640, -1062694792], s, SIMD.Int32x4, "func24")
}

var ret25 = m.func25(s1, s2);
var ret25 = m.func25(s1, s2);
var ret25 = m.func25(s1, s2);

ret25 = SIMD.Uint32x4.fromInt32x4Bits(ret25);
//printSimdBaseline(ret25, "SIMD.Uint32x4", "ret25", "func25");
equalSimd([4294967295, 4294967294, 0, 0], ret25, SIMD.Uint32x4, "func25")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
//    printSimdBaseline(s, "SIMD.Int32x4", "s", "func25");
    equalSimd([-1, -2, 1615343640, -1062694792], s, SIMD.Int32x4, "func25")
}

var ret26 = m.func26(s1, s2);
var ret26 = m.func26(s1, s2);
var ret26 = m.func26(s1, s2);
ret26 = SIMD.Uint32x4.fromInt32x4Bits(ret26);
//printSimdBaseline(ret26, "SIMD.Uint32x4", "ret26", "func26");
equalSimd([4294967295, 4294967294, 4294967293, 0], ret26, SIMD.Uint32x4, "func26")

for (i = 25; i < 25 + 4 * 3; i += 4)
{
    var s = SIMD.Int32x4.load(Int32Heap, i);
    //printSimdBaseline(s, "SIMD.Int32x4", "s", "func26");
    equalSimd([-1, -2, -3, -1062694792], s, SIMD.Int32x4, "func26")
}

/*
printSimdBaseline(ret1, "SIMD.Int16x8", "ret1", "func1");
printSimdBaseline(ret2, "SIMD.Int16x8", "ret2", "func2");
printSimdBaseline(ret3, "SIMD.Uint16x8", "ret3", "func3");
printSimdBaseline(ret4, "SIMD.Uint32x4", "ret4", "func4");
printSimdBaseline(ret5, "SIMD.Uint8x16", "ret5", "func5");
*/
/*
printSimdBaseline(ret8, "SIMD.Uint32x4",  "ret8", "func8");
printSimdBaseline(ret9, "SIMD.Int16x8",   "ret9", "func9");
printSimdBaseline(ret10, "SIMD.Uint16x8", "ret10", "func10");
printSimdBaseline(ret11, "SIMD.Uint8x16", "ret11", "func11");
*/
/*
printSimdBaseline(ret12, "SIMD.Uint32x4",  "ret12", "func12");
printSimdBaseline(ret13, "SIMD.Int16x8",   "ret13", "func13");
printSimdBaseline(ret14, "SIMD.Uint16x8", "ret14", "func14");
printSimdBaseline(ret15, "SIMD.Uint8x16", "ret15", "func15");

printSimdBaseline(ret16, "SIMD.Uint32x4",  "ret16", "func16");
printSimdBaseline(ret17, "SIMD.Int16x8",   "ret17", "func17");
printSimdBaseline(ret18, "SIMD.Uint16x8", "ret18", "func18");
printSimdBaseline(ret19, "SIMD.Uint8x16", "ret19", "func19");
*/

/*
printSimdBaseline(ret21, "SIMD.Uint8x16", "ret21", "func21");
printSimdBaseline(ret22, "SIMD.Uint8x16", "ret22", "func22");
printSimdBaseline(ret23, "SIMD.Uint8x16", "ret23", "func23");
printSimdBaseline(ret24, "SIMD.Uint8x16", "ret24", "func24");
printSimdBaseline(ret25, "SIMD.Uint8x16", "ret25", "func25");
printSimdBaseline(ret26, "SIMD.Uint8x16", "ret26", "func26");
*/
equalSimd([2, -3, 4, 5, 6, -7, 8, 9], ret1, SIMD.Int16x8, "func1")
equalSimd([2, 4, 13, 5, 15, 7, 17, 9], ret2, SIMD.Int16x8, "func2")
ret3 = SIMD.Uint16x8.fromInt16x8Bits(ret3);
equalSimd([0, 65535, 2, 3, 65535, 5, 65436, 0], ret3, SIMD.Uint16x8, "func3")
ret4 = SIMD.Uint32x4.fromInt32x4Bits(ret4);
equalSimd([0, 4294967295, 2, 4294967295], ret4, SIMD.Uint32x4, "func4");
ret5 = SIMD.Uint8x16.fromInt8x16Bits(ret5);
equalSimd([0, 255, 2, 3, 255, 5, 156, 0, 1, 255, 3, 4, 156, 7, 156, 0], ret5, SIMD.Uint8x16, "func5")

equal(3975, ret6);
equal(524216, ret7);

ret8 = SIMD.Uint32x4.fromInt32x4Bits(ret8);
equalSimd([4294967295, 4294967294, 4294967293, 4294967292], ret8, SIMD.Uint32x4, "func8")
equalSimd([-1, -2, -3, -4, -5, -6, -7, -8], ret9, SIMD.Int16x8, "func9")
ret10 = SIMD.Uint16x8.fromInt16x8Bits(ret10);
equalSimd([65535, 65534, 65533, 65532, 65531, 65530, 65529, 65528], ret10, SIMD.Uint16x8, "func10")
ret11 = SIMD.Uint8x16.fromInt8x16Bits(ret11);
equalSimd([255, 254, 253, 252, 251, 250, 249, 248, 255, 254, 253, 252, 251, 250, 249, 248], ret11, SIMD.Uint8x16, "func11")
ret12 = SIMD.Uint32x4.fromInt32x4Bits(ret12);
equalSimd([4294967292, 4294967293, 4294967294, 4294967295], ret12, SIMD.Uint32x4, "func12")
equalSimd([-4, -3, -2, -1, -8, -7, -6, -5], ret13, SIMD.Int16x8, "func13")

ret14 = SIMD.Uint16x8.fromInt16x8Bits(ret14);
equalSimd([65528, 65529, 65530, 65531, 65532, 65533, 65534, 65535], ret14, SIMD.Uint16x8, "func14")
ret15 = SIMD.Uint8x16.fromInt8x16Bits(ret15);
equalSimd([248, 249, 250, 251, 252, 253, 254, 255, 248, 249, 250, 251, 252, 253, 254, 255], ret15, SIMD.Uint8x16, "func15")
ret16 = SIMD.Uint32x4.fromInt32x4Bits(ret16);
equalSimd([4294966896, 4294966996, 4294967292, 4294967294], ret16, SIMD.Uint32x4, "func16")
equalSimd([-800, -700, -600, -500, -8, -7, -6, -5], ret17, SIMD.Int16x8, "func17")
ret18 = SIMD.Uint16x8.fromInt16x8Bits(ret18);
equalSimd([64736, 64836, 64936, 65036, 65528, 65529, 65530, 65531], ret18, SIMD.Uint16x8, "func18")
ret19 = SIMD.Uint8x16.fromInt8x16Bits(ret19);
equalSimd([248, 249, 250, 251, 252, 253, 254, 255, 224, 68, 168, 12, 252, 253, 254, 255], ret19, SIMD.Uint8x16, "func19")



print("PASS");

