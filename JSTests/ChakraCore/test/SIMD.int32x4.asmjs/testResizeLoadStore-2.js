//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports, buffer) {
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
    var i4swizzle = i4.swizzle;
    var i4shuffle = i4.shuffle;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;
    var i4load  = i4.load;
    var i4load1 = i4.load1;
    var i4load2 = i4.load2;
    var i4load3 = i4.load3;
    
    var i4store  = i4.store
    var i4store1 = i4.store1;
    var i4store2 = i4.store2;
    var i4store3 = i4.store3;
    
    //var i4shiftLeftByScalar = i4.shiftLeftByScalar;
    //var i4shiftRightByScalar = i4.shiftRightByScalar;
    //var i4shiftRightArithmeticByScalar = i4.shiftRightArithmeticByScalar;
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
    var f4swizzle = f4.swizzle;
    var f4shuffle = f4.shuffle;
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;
    
    var f4load = f4.load;
    var f4load1 = f4.load1;
    var f4load2 = f4.load2;
    var f4load3 = f4.load3;
    
    var f4store  = f4.store;
    var f4store1 = f4.store1;
    var f4store2 = f4.store2;
    var f4store3 = f4.store3;
    
    
    
    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportI4 = i4check(imports.g2);       // global var import
    
    var g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var g2 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    
    var gval = 1234;
    var gval2 = 1234.0;

    var loopCOUNT = 3;
    
    var Int8Array = stdlib.Int8Array;
    var Uint8Array = stdlib.Uint8Array;
    var Int16Array = stdlib.Int16Array;
    var Uint16Array = stdlib.Uint16Array;
    var Int32Array = stdlib.Int32Array;
    var Uint32Array = stdlib.Uint32Array;
    var Float32Array = stdlib.Float32Array;
    
    var Int8Heap = new stdlib.Int8Array (buffer);    
    var Uint8Heap = new stdlib.Uint8Array (buffer);   
    var Int16Heap = new stdlib.Int16Array(buffer);
    var Uint16Heap = new stdlib.Uint16Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var Uint32Heap = new stdlib.Uint32Array(buffer);
    var Float32Heap = new stdlib.Float32Array(buffer);    

    var len=stdlib.byteLength;
    
    function ch(newBuffer) 
    { 
        if(len(newBuffer) & 0xffffff || len(newBuffer) <= 0xffffff || len(newBuffer) > 0x80000000) 
            return false; 
        
        Int8Heap = new Int8Array(newBuffer);
        Uint8Heap = new Uint8Array(newBuffer);
        Int16Heap = new Int16Array(newBuffer);
        Uint16Heap = new Uint16Array(newBuffer);
        Int32Heap = new Int32Array(newBuffer);
        Uint32Heap = new Uint32Array(newBuffer);
        Float32Heap = new Float32Array(newBuffer);
        
        buffer=newBuffer; 
        return true;
    }
    
     function storeF32(value, idx) 
    {
        value= i4check(value);
        idx = idx|0;
        idx = idx<<2;
        i4store(Float32Heap, (idx>>2), value);
    }
    function loadF32(idx) 
    {
        idx = idx|0;
        idx = idx<<2;
        return i4load(Float32Heap, (idx>>2));
    }
    
    function storeUI32(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<2; 
    i4store(Uint32Heap, (idx>>2), value);}
    function loadUI32(idx) 
    { idx = idx|0; idx = idx<<2; return i4load(Uint32Heap, (idx>>2)); }
    
    function storeI32(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<2; 
    i4store(Int32Heap, (idx>>2), value);}
    function loadI32(idx) 
    { idx = idx|0; idx = idx<<2; return i4load(Int32Heap, (idx>>2)); }
    
    function storeI16(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<1; 
    i4store(Int16Heap, (idx>>1), value);}
    function loadI16(idx) 
    { idx = idx|0; idx = idx<<1; return i4load(Int16Heap, (idx>>1)); }

    function storeUI16(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<1; 
    i4store(Uint16Heap, (idx>>1), value);}
    function loadUI16(idx) 
    { idx = idx|0; idx = idx<<1; return i4load(Uint16Heap, (idx>>1)); }

    function storeI8(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<0; 
    i4store(Int8Heap, (idx>>0), value);}
    function loadI8(idx) 
    { idx = idx|0; idx = idx<<0; return i4load(Int8Heap, (idx>>0)); }

    function storeUI8(value, idx) 
    { value= i4check(value); idx = idx|0; idx = idx<<0; 
    i4store(Uint8Heap, (idx>>0), value);}
    function loadUI8(idx) 
    { idx = idx|0; idx = idx<<0; return i4load(Uint8Heap, (idx>>0)); }
    
    
    function loadStoreIndex1()
    {
        i4store(Float32Heap, 0, i4(-1,-2,3,-4)); 
        return i4load(Float32Heap, 0);
    }
    
    
    function store_1(functionPicker) //Function picker to pick store1/store2/store3/store
    {
        functionPicker = functionPicker|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 256;        
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = 0;
            v0 = i4(5,-12,0,2220);
            for(idx = idx << 2; (idx|0) < (end|0 << 2); idx = (idx + 16)|0)
            {
                switch(functionPicker|0)
                {
                    case 5:
                        i4store(Float32Heap, idx>>2, v0); 
                        break;
                    case 6:
                        i4store1(Float32Heap, idx>>2, v0);
                        break;
                    case 7:
                        i4store2(Float32Heap, idx>>2, v0);
                        break;
                    case 8:
                        i4store3(Float32Heap, idx>>2, v0);
                        break;
                    default:
                        break;
                }
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 0);
        //Alternate validation
        // for(idx = 0; idx << 2 < end << 2; idx = (idx + 4 << 2)|0)
        // {
            // verify buffer view is as expected. 
            // 0,0,0,0,1,1,1,1,2,2,2,2... 
        // }
    }        
     function store_2(functionPicker)
    {
        functionPicker = functionPicker|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 256;        
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            idx = 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 2; (idx|0) < (end|0 << 2); idx = (idx + 16)|0)
            {
                switch(functionPicker|0)
                {
                    case 5:
                        i4store(Float32Heap, idx>>2, v0); 
                        break;
                    case 6:
                        i4store1(Float32Heap, idx>>2, v0);
                        break;
                    case 7:
                        i4store2(Float32Heap, idx>>2, v0);
                        break;
                    case 8:
                        i4store3(Float32Heap, idx>>2, v0);
                        break;
                    default:
                        break;
                }
                v0 = i4add(v0, i4(1,1,1,1));
            }
        }
        return i4load(Float32Heap, 8);
    } 
    function store_3(functionPicker)
    {
        functionPicker = functionPicker|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 256;    

        loopIndex = loopCOUNT | 0;
        do {
            idx = 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 2; (idx|0) < (end|0 << 2); idx = (idx + 16)|0)
            {
                switch(functionPicker|0)
                {
                    case 5:
                        i4store(Float32Heap, idx>>2, v0); 
                        break;
                    case 6:
                        i4store1(Float32Heap, idx>>2, v0);
                        break;
                    case 7:
                        i4store2(Float32Heap, idx>>2, v0);
                        break;
                    case 8:
                        i4store3(Float32Heap, idx>>2, v0);
                        break;
                    default:
                        break;
                }
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
        return i4load(Float32Heap, 8);
    } 
    function store_1_Int8(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 0; (idx|0) < (end|0 << 0); idx = (idx + 16)|0)
            {
                i4store(Int8Heap, idx>>0, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }
    function store_1_Uint8(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 0; (idx|0) < (end|0 << 0); idx = (idx + 16)|0)
            {
                i4store(Uint8Heap, idx>>0, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }
    function store_1_Int16(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 1; (idx|0) < (end|0 << 1); idx = (idx + 16)|0)
            {
                i4store(Int16Heap, idx>>1, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }    
    function store_1_Uint16(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 1; (idx|0) < (end|0 << 1); idx = (idx + 16)|0)
            {
                i4store(Uint16Heap, idx>>1, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }    
    function store_1_Int32(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 2; (idx|0) < (end|0 << 2); idx = (idx + 16)|0)
            {
                i4store(Int32Heap, idx>>2, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }    
    function store_1_Uint32(length) 
    {
        length = length|0;
        var v0 = i4(0,0,0,0);
        var loopIndex = 0, idx = 0, end = 0;
        end = (length * 4)|0; 
        while((loopIndex|0) < (loopCOUNT|0)) 
        {
            idx = (end - 4096) | 0;
            v0 = i4(0,0,0,0);
            for(idx = idx << 2; (idx|0) < (end|0 << 2); idx = (idx + 16)|0)
            {
                i4store(Uint32Heap, idx>>2, v0); 
                v0 = i4add(v0, i4(1,1,1,1));
            }
            loopIndex = (loopIndex + 1)|0;
        }
        //Expects the heap to be: 0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
        return i4load(Float32Heap, 2);
    }
    
    ////////////////////////////Load////////////////////////////
    function load_1(functionPicker)
    {
        //length = length|0;
        functionPicker = functionPicker|0;
        
        var idx=0,end=16;//(length-4)|0;;
        var loopIndex = 0;
        var v = i4(0,0,0,0);

        while ( (loopIndex|0) < (loopCOUNT|0)) {
            
            idx=0;
            
            for(idx = idx<<2; (idx|0) <= (end<<2); idx = (idx + 1)|0)
            {        
                switch(functionPicker|0)
                {
                    case 1:
                        v = i4load(Float32Heap, idx>>2); 
                        break;
                     case 2:
                        v = i4load1(Float32Heap, idx>>2);
                        break;
                    case 3:
                        v = i4load2(Float32Heap, idx>>2);
                        break;
                    case 4:
                        v = i4load3(Float32Heap, idx>>2);
                        break;
                    default:
                        break;
                }
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
     
    function load_2(functionPicker)
    {
        //length = length|0;
        functionPicker = functionPicker|0;
        
        var idx=0,end=16;//(length-4)|0;;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
             idx=0;
            
            for(idx = idx<<2; (idx|0) <= (end<<2); idx = (idx + 1)|0)
            {        
                switch(functionPicker|0)
                {
                    case 1:
                        v = i4load(Float32Heap, idx>>2); 
                        break;
                     case 2:
                        v = i4load1(Float32Heap, idx>>2);
                        break;
                    case 3:
                        v = i4load2(Float32Heap, idx>>2);
                        break;
                    case 4:
                        v = i4load3(Float32Heap, idx>>2);
                        break;
                    default:
                        break;
                }
            }
        }
        return v;
    }

    function load_3(functionPicker)
    {
        //length = length|0;
        functionPicker = functionPicker|0;
        
        var idx=0,end=16;//(length-4)|0;;
        var loopIndex = 0;
        var v = i4(0,0,0,0);

        loopIndex = loopCOUNT | 0;
        do {
            idx = 0;
            for(idx = idx<<2; (idx|0) <= (end<<2); idx = (idx + 1)|0)
            {        
                switch(functionPicker|0)
                {
                    case 1:
                        v = i4load(Float32Heap, idx>>2); 
                        break;
                     case 2:
                        v = i4load1(Float32Heap, idx>>2);
                        break;
                    case 3:
                        v = i4load2(Float32Heap, idx>>2);
                        break;
                    case 4:
                        v = i4load3(Float32Heap, idx>>2);
                        break;
                    default:
                        break;
                }
            }
            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);
        return v;
    } 
    function load_1_Int8(length)
    {
        length = length|0;
        var idx=0,end=0;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 4)|0) - 16)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (end - 4096) | 0;
            for(idx = idx<<0; (idx|0) <= (end<<0); idx = (idx + 1)|0)
            {        
                v = i4load(Int8Heap, idx>>0); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
    function load_1_Uint8(length)
    {
        length = length|0;
        var idx=0,end=0;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 4)|0) - 16)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (((length * 4)|0) - 4096) | 0;
            for(idx = idx<<0; (idx|0) <= (end<<0); idx = (idx + 1)|0)
            {        
                v = i4load(Uint8Heap, idx>>0); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
    function load_1_Int16(length)
    {
        length = length|0;
        var idx=0,end=0;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 2)|0) - 8)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (end - 4096) | 0;
            for(idx = idx<<1; (idx|0) <= (end<<1); idx = (idx + 1)|0)
            {        
                v = i4load(Int16Heap, idx>>1); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
    function load_1_Uint16(length)
    {
        length = length|0;
        var idx=0,end=120;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 2)|0) - 8)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (end - 4096) | 0;
            for(idx = idx<<1; (idx|0) <= (end<<1); idx = (idx + 1)|0)
            {        
                v = i4load(Uint16Heap, idx>>1); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
    function load_1_Int32(length)
    {
        length = length|0;
        var idx=0,end=60;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 1)|0) - 4)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (end - 4096) | 0;
            for(idx = idx<<2; (idx|0) <= (end<<2); idx = (idx + 1)|0)
            {        
                v = i4load(Int32Heap, idx>>2); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }
    function load_1_Uint32(length)
    {
        length = length|0;
        var idx=0,end=60;
        var loopIndex = 0;
        var v = i4(0,0,0,0);
        end = (((length * 1)|0) - 4)|0; 
        while ( (loopIndex|0) < (loopCOUNT|0)) {
            idx= (end - 4096) | 0;
            for(idx = idx<<2; (idx|0) <= (end<<2); idx = (idx + 1)|0)
            {        
                v = i4load(Uint32Heap, idx>>2); 
            }
            loopIndex = (loopIndex + 1) | 0;
        }
        return v;
    }    
    
    return {
            changeHeap:ch
           ,store1:store_1
           ,store2:store_2
           ,store3:store_3
           ,store1Int8:store_1_Int8
           ,store1Uint8:store_1_Uint8
           ,store1Int16:store_1_Int16
           ,store1Uint16:store_1_Uint16
           ,store1Int32:store_1_Int32
           ,store1Uint32:store_1_Uint32
           ,load1:load_1
           ,load2:load_2
           ,load3:load_3
           ,load1Int8:load_1_Int8
           ,load1Uint8:load_1_Uint8
           ,load1Int16:load_1_Int16
           ,load1Uint16:load_1_Uint16
           ,load1Int32:load_1_Int32
           ,load1Uint32:load_1_Uint32
           ,loadF32:loadF32
           ,storeF32:storeF32
           ,storeUI32:storeUI32
           ,loadUI32:loadUI32
           ,storeI32:storeI32
           ,loadI32:loadI32           
           ,storeI16:storeI16
           ,loadI16:loadI16
           ,storeUI16:storeUI16
           ,loadUI16:loadUI16
           ,storeI8:storeI8
           ,loadI8:loadI8    
           ,storeUI8:storeUI8
           ,loadUI8:loadUI8           
           ,loadStoreIndex1:loadStoreIndex1};
}

var buffer = new ArrayBuffer(0x1000000); //16mb min 2^12

//Reset or flush the buffer
function initF32(buffer) {
    var values = new Float32Array( buffer );
    for( var i=0; i < values.length ; ++i ) {
        values[i] = i * 10;
    }
    return values.length;
}
function printBuffer(buffer, count)
{
    var i4;
    for (var i = 0; i < count/* * 16*/; i += 16)
    {
        i4 = SIMD.Int32x4.load(buffer, i);
        print(i4.toString());
    }
}

function printResults(res)
{
    print(typeof(res));
    print(res.toString());
}

inputLength = initF32(buffer);
print(inputLength);
//Enumerating SIMD loads to test. 
SIMDLoad = 1;
SIMDLoad1 = 2;
SIMDLoad2 = 3;
SIMDLoad3 = 4;

SIMDStore = 5;
SIMDStore1 = 6;
SIMDStore2 = 7;
SIMDStore3 = 8;

//Module initialization
this['byteLength'] =
  Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
var m = asmModule(this, {g0:initF32(buffer),g1:SIMD.Float32x4(9,9,9,9), g2:SIMD.Int32x4(1, 2, 3, 4)}, buffer);
var values = new Float32Array(buffer);

print("Stores:");

print("Test1");
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore1);//Lane1 store
equalSimd([5, 1092616192, 1101004800, 1106247680], ret, SIMD.Int32x4, "");

print("Test2");;
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore2);//Lane 1,2 store
equalSimd([5, -12, 1101004800, 1106247680], ret, SIMD.Int32x4, "");

print("Test3");
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore3);//Lane 1,2,3 store
equalSimd([5, -12, 0, 1106247680], ret, SIMD.Int32x4, "");

print("Test4");
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore);//Generic Store
equalSimd([5, -12, 0, 2220], ret, SIMD.Int32x4, "");

print("Test5");
inputLength = initF32(buffer);  
var ret = m.store2(SIMDStore);//Generic store 
equalSimd([2, 2, 2, 2], ret, SIMD.Int32x4, "");

print("Test6");
inputLength = initF32(buffer); 
var ret = m.store3(SIMDStore);//Generic store
equalSimd([2, 2, 2, 2], ret, SIMD.Int32x4, "");

print("Test7");
inputLength = initF32(buffer); 
var ret = m.store1Int8(inputLength);//Int8Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test8");
inputLength = initF32(buffer); 
var ret = m.store1Uint8(inputLength);//Uint8Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test9");
inputLength = initF32(buffer); 
var ret = m.store1Int16(inputLength);//Int16Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test10");
inputLength = initF32(buffer); 
var ret = m.store1Uint16(inputLength);//Uint16Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test12");
inputLength = initF32(buffer); 
var ret = m.store1Int32(inputLength);//Int32Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test13");
inputLength = initF32(buffer); 
var ret = m.store1Uint32(inputLength);//Uint32Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test14");
inputLength = initF32(buffer); 
var ret = m.loadStoreIndex1();//Uint32Heap store
equalSimd([-1, -2, 3, -4], ret, SIMD.Int32x4, "");

print("Loads");
print("Test1");
var ret = m.load1(SIMDLoad1);
equalSimd([1126170624, 0, 0, 0], ret, SIMD.Int32x4, "");

print("Test2");
var ret = m.load1(SIMDLoad2);
equalSimd([1126170624, 1126825984, 0, 0], ret, SIMD.Int32x4, "");

print("Test3");
var ret = m.load1(SIMDLoad3);
equalSimd([1126170624, 1126825984, 1127481344, 0], ret, SIMD.Int32x4, "");

print("Test4");
var ret = m.load1(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test5");
var ret = m.load2(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test6");
var ret = m.load3(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test7");
var ret = m.load1Int8(inputLength); //Int8Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("Test8");
var ret = m.load1Uint8(inputLength); //Int8Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("Test9");
var ret = m.load1Int16(inputLength); //Int16Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("Test10");
var ret = m.load1Uint16(inputLength); //Int16Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("Test11");
var ret = m.load1Int32(inputLength); //Int32Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("Test12");
var ret = m.load1Uint32(inputLength); //Int32Heap load
equalSimd([1277165558, 1277165560, 1277165563, 1277165566], ret, SIMD.Int32x4, "");

print("BoundCheck");
var value = SIMD.Int32x4(9.9,1.2,3.4,5.6);

print("Test1");
try {m.storeF32(value, inputLength); print("Wrong");} catch(err) {print("Correct");}

print("Test2");
try {m.loadF32(inputLength); print("Wrong");} catch(err) {print("Correct");}

print("Test3");
try {m.storeF32(value, inputLength-1); print("Wrong");} catch(err) {print("Correct");}

print("Test4");
try {m.loadF32(inputLength-1); print("Wrong");} catch(err) {print("Correct");}

print("Test5");
try {m.storeF32(value, inputLength-4);print("Correct");} catch(err) {print("Wrong");}

print("Test6");
try {var v = m.loadF32(inputLength-4);print("Correct");} catch(err) {print("Wrong");}

print("Test7");
try {m.storeUI32(value, inputLength+1);print("Wrong");} catch(err) {print("Correct");}

print("Test8");
try { m.loadUI32(inputLength+1); print("Wrong"); } catch(err) { print("Correct"); }

print("Test9");
try {m.storeI32(value, inputLength+1); print("Wrong");} catch(err) {print("Correct");}

print("Test10");
try {m.loadI32(inputLength+1);print("Wrong");} catch(err) {print("Correct");}

print("Test11");
try{
    m.storeI16(value, inputLength*2-8);
    print("Correct");
    m.storeUI16(value, inputLength*2-8);
    print("Correct");
    m.storeI8(value, inputLength*4-16);
    print("Correct");
    m.storeUI8(value, inputLength*4-16);
    print("Correct");
    m.loadI16(inputLength*2-8);
    print("Correct");
    m.loadUI16(inputLength*2-8);
    print("Correct");
    m.loadI8(inputLength*4-16);
    print("Correct");
    m.loadUI8(inputLength*4-16);
    print("Correct");
} catch(err){ print("Wrong"); }

print("Test12");
try {m.storeUI16(value, inputLength*2);print("Wrong");} catch(err) {print("Correct");}

print("Test13");
try {m.loadUI16(inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test14");
try {m.storeI16(value, inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test15");
try {m.loadI16(inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test16");
try {m.storeUI8(value, inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test17");
try {m.loadUI8(inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test18");
try {m.storeI8(value, inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test19");
try {m.loadI8(inputLength*4+15); print("Wrong");} catch(err) {print("Correct");}

// change buffer
var buffer = new ArrayBuffer(0x2000000);
print(m.changeHeap(buffer));

print(">>> ChangeHeap ");

print("Stores:");

print("Test1");
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore1);//Lane1 store
equalSimd([5, 1092616192, 1101004800, 1106247680], ret, SIMD.Int32x4, "");

print("Test2");;
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore2);//Lane 1,2 store
equalSimd([5, -12, 1101004800, 1106247680], ret, SIMD.Int32x4, "");

print("Test3");
inputLength = initF32(buffer); 
var ret = m.store1(SIMDStore3);//Lane 1,2,3 store
equalSimd([5, -12, 0, 1106247680], ret, SIMD.Int32x4, "");

print("Test4");
inputLength = initF32(buffer); 
//Should change the buffer to  0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3...15,15,15,15,0,0,0,0...
var ret = m.store1(SIMDStore);//Generic Store
equalSimd([5, -12, 0, 2220], ret, SIMD.Int32x4, "");

print("Test5");
inputLength = initF32(buffer);  
var ret = m.store2(SIMDStore);//Generic store 
equalSimd([2, 2, 2, 2], ret, SIMD.Int32x4, "");

print("Test6");
inputLength = initF32(buffer); 
var ret = m.store3(SIMDStore);//Generic store
equalSimd([2, 2, 2, 2], ret, SIMD.Int32x4, "");

print("Test7");
inputLength = initF32(buffer); 
var ret = m.store1Int8(inputLength);//Int8Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test8");
inputLength = initF32(buffer); 
var ret = m.store1Uint8(inputLength);//Uint8Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test9");
inputLength = initF32(buffer); 
var ret = m.store1Int16(inputLength);//Int16Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test10");
inputLength = initF32(buffer); 
var ret = m.store1Uint16(inputLength);//Uint16Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test12");
inputLength = initF32(buffer); 
var ret = m.store1Int32(inputLength);//Int32Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test13");
inputLength = initF32(buffer); 
var ret = m.store1Uint32(inputLength);//Uint32Heap store
equalSimd([1101004800, 1106247680, 1109393408, 1112014848], ret, SIMD.Int32x4, "");

print("Test14");
inputLength = initF32(buffer); 
var ret = m.loadStoreIndex1();//Uint32Heap store
equalSimd([-1, -2, 3, -4], ret, SIMD.Int32x4, "");


print("Loads");
print("Test1");
var ret = m.load1(SIMDLoad1);
equalSimd([1126170624, 0, 0, 0], ret, SIMD.Int32x4, "");

print("Test2");
var ret = m.load1(SIMDLoad2);
equalSimd([1126170624, 1126825984, 0, 0], ret, SIMD.Int32x4, "");

print("Test3");
var ret = m.load1(SIMDLoad3);
equalSimd([1126170624, 1126825984, 1127481344, 0], ret, SIMD.Int32x4, "");

print("Test4");
var ret = m.load1(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test5");
var ret = m.load2(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test6");
var ret = m.load3(SIMDLoad);
equalSimd([1126170624, 1126825984, 1127481344, 1128136704], ret, SIMD.Int32x4, "");

print("Test7");
var ret = m.load1Int8(inputLength); //Int8Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("Test8");
var ret = m.load1Uint8(inputLength); //Int8Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("Test9");
var ret = m.load1Int16(inputLength); //Int16Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("Test10");
var ret = m.load1Uint16(inputLength); //Int16Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("Test11");
var ret = m.load1Int32(inputLength); //Int32Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("Test12");
var ret = m.load1Uint32(inputLength); //Int32Heap load
equalSimd([1285554171, 1285554172, 1285554174, 1285554175], ret, SIMD.Int32x4, "");

print("BoundCheck");
var value = SIMD.Int32x4(9.9,1.2,3.4,5.6);

print("Test1");
try {m.storeF32(value, inputLength); print("Wrong");} catch(err) {print("Correct");}

print("Test2");
try {m.loadF32(inputLength); print("Wrong");} catch(err) {print("Correct");}

print("Test3");
try {m.storeF32(value, inputLength-1); print("Wrong");} catch(err) {print("Correct");}

print("Test4");
try {m.loadF32(inputLength-1); print("Wrong");} catch(err) {print("Correct");}

print("Test5");
try {m.storeF32(value, inputLength-4);print("Correct");} catch(err) {print("Wrong");}

print("Test6");
try {var v = m.loadF32(inputLength-4);print("Correct");} catch(err) {print("Wrong");}

print("Test7");
try {m.storeUI32(value, inputLength+1);print("Wrong");} catch(err) {print("Correct");}

print("Test8");
try { m.loadUI32(inputLength+1); print("Wrong"); } catch(err) { print("Correct"); }

print("Test9");
try {m.storeI32(value, inputLength+1); print("Wrong");} catch(err) {print("Correct");}

print("Test10");
try {m.loadI32(inputLength+1);print("Wrong");} catch(err) {print("Correct");}

print("Test11");
try{
    m.storeI16(value, inputLength*2-8);
    print("Correct");
    m.storeUI16(value, inputLength*2-8);
    print("Correct");
    m.storeI8(value, inputLength*4-16);
    print("Correct");
    m.storeUI8(value, inputLength*4-16);
    print("Correct");
    m.loadI16(inputLength*2-8);
    print("Correct");
    m.loadUI16(inputLength*2-8);
    print("Correct");
    m.loadI8(inputLength*4-16);
    print("Correct");
    m.loadUI8(inputLength*4-16);
    print("Correct");
} catch(err){ print("Wrong"); }

print("Test12");
try {m.storeUI16(value, inputLength*2);print("Wrong");} catch(err) {print("Correct");}

print("Test13");
try {m.loadUI16(inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test14");
try {m.storeI16(value, inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test15");
try {m.loadI16(inputLength*2-7); print("Wrong");} catch(err) {print("Correct");}

print("Test16");
try {m.storeUI8(value, inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test17");
try {m.loadUI8(inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test18");
try {m.storeI8(value, inputLength*4-15); print("Wrong");} catch(err) {print("Correct");}

print("Test19");
try {m.loadI8(inputLength*4+15); print("Wrong");} catch(err) {print("Correct");}
print("PASS");