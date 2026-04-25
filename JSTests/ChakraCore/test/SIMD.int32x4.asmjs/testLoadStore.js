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

    var Int8Heap = new stdlib.Int8Array (buffer);    
    var Uint8Heap = new stdlib.Uint8Array (buffer);    

    var Int16Heap = new stdlib.Int16Array(buffer);
    var Uint16Heap = new stdlib.Uint16Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var Uint32Heap = new stdlib.Uint32Array(buffer);
    var Float32Heap = new stdlib.Float32Array(buffer);

    function func0()
    {
        var x  = i4(1, 2, 3, 4);
        var y  = i4(0, 0, 0, 0);
        var st = i4(0, 0, 0, 0);
        var ld = i4(0, 0, 0, 0);
        
        var t0 = i4(0, 0, 0, 0);
        var t1 = i4(0, 0, 0, 0);
        var t2 = i4(0, 0, 0, 0);
        var t3 = i4(0, 0, 0, 0);
        
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            st = i4store(Int8Heap, index >> 0, x);
            ld = i4load(Int8Heap, index >> 0);
            y = i4add(st, ld);  // (0,0,0,0)
            t0 = i4add(i4store(Int8Heap, index >> 0, x), i4load(Int8Heap, index   >> 0));
            t0 = i4add(y, t0);
            
            st = i4store1(Int8Heap, index >> 0, x);
            ld = i4load(Int8Heap, index >> 0);
            y = i4add(st, ld);  // (0,0,0,0)
            t1 = i4add(i4store1(Int8Heap, index >> 0, x), i4load(Int8Heap, index   >> 0));
            t1 = i4add(y, t1);
             
            st = i4store2(Int8Heap, index >> 0, x);
            ld = i4load(Int8Heap, index >> 0);
            y = i4add(st, ld);  // (0,0,0,0)
            t2 = i4add(i4store2(Int8Heap, index >> 0, x), i4load(Int8Heap, index   >> 0));
            t2 = i4add(y, t2);

            st = i4store3(Int8Heap, index >> 0, x);
            ld = i4load(Int8Heap, index >> 0);
            y = i4add(st, ld);  // (0,0,0,0)
            t3 = i4add(i4store3(Int8Heap, index >> 0, x), i4load(Int8Heap, index   >> 0));
            t3 = i4add(y, t3);

            t0 = i4add(t0, i4add(t1, i4add(t2, t3)));
            index = (index + 16 ) | 0;
        }
        return i4check(t0);
    }

    function func1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0, 0, 0, 0);
        var y = i4(0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load(Uint8Heap, index >> 0);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func1OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }

        // No OOB
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load(Uint8Heap, index >> 0);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }
    
    function func1OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        // OOB
        index = ((0x10000-160) + 1)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load(Uint8Heap, index >> 0);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store3(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load3(Int16Heap, index >> 1);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func2OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store3(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        // No OOB here
        index = ((0x10000 - 160) + 4)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load3(Int16Heap, index >> 1);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func2OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store3(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 6)|0;
        // OOB here
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load3(Int16Heap, index >> 1);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
    }

    function func3()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store2(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Int32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func3OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store2(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 8)|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Int32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func3OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store2(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 32)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Int32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func4()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store1(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func4OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store1(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 12)|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func4OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store1(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func5()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func5OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 8)|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func5OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 12)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load2(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func6()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func6OOB_1()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 12)|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    function func6OOB_2()
    {
        var x = i4(1, 2, 3, 4);
        var t = i4(0,0,0,0);
        var y = i4(0,0,0,0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i4store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i4load1(Float32Heap, index >> 2);
            y = i4add(y, t);
            index = (index + 16 ) | 0;
        }
        return i4check(y);
    }

    // TODO: Test conversion of returned value
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

    return {
        func0:func0,
        func1:func1, 
        func1OOB_1:func1OOB_1, 
        func1OOB_2:func1OOB_2, 
        
        func2:func2, 
        func2OOB_1:func2OOB_1, 
        func2OOB_2:func2OOB_2, 
        
        func3:func3, 
        func3OOB_1:func3OOB_1, 
        func3OOB_2:func3OOB_2, 
        
        func4:func4, 
        func4OOB_1:func4OOB_1, 
        func4OOB_2:func4OOB_2, 
        
        func5:func5, 
        func5OOB_1:func5OOB_1, 
        func5OOB_2:func5OOB_2, 
        
        func6:func6,
        func6OOB_1:func6OOB_1,
        func6OOB_2:func6OOB_2
        };
}
var buffer = new ArrayBuffer(0x10000);
var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Int32x4(-1065353216, -1073741824,-1077936128, -1082130432)}, buffer);

var ret;

ret = m.func0();
equalSimd([16, 32, 48, 64], ret, SIMD.Int32x4, "Test Load Store");

ret = m.func1();
equalSimd([10, 20, 30, 40], ret, SIMD.Int32x4, "Test Load Store1");


ret = m.func2();

equalSimd([10, 20, 30, 0], ret, SIMD.Int32x4, "Test Load Store2");


ret = m.func3();
equalSimd([10, 20, 0, 0], ret, SIMD.Int32x4, "Test Load Store3");


ret = m.func4();
equalSimd([10, 0, 0, 0], ret, SIMD.Int32x4, "Test Load Store4");

ret = m.func5();
equalSimd([10, 20, 0, 0], ret, SIMD.Int32x4, "Test Load Store5");


ret = m.func6();
equalSimd([10, 0, 0, 0], ret, SIMD.Int32x4, "Test Load Store6");

//

var funcOOB1 = [m.func1OOB_1, m.func2OOB_1 ,m.func3OOB_1, m.func4OOB_1, m.func5OOB_1, m.func6OOB_1];
var RESULTS = [SIMD.Int32x4(10, 20, 30, 40),
SIMD.Int32x4(20, 30, 40, 0),
SIMD.Int32x4(30, 40, 0, 0),
SIMD.Int32x4(40, 0, 0, 0),
SIMD.Int32x4(30, 40, 0, 0),
SIMD.Int32x4(40, 0, 0, 0)
];

for (var i = 0; i < funcOOB1.length; i ++)
{
    try
    {
        ret = funcOOB1[i]();
        equalSimd(RESULTS[i], ret, SIMD.Int32x4, "Test Load Store");

    } catch(e)
    {
        print("Wrong");
    }
}

//
var funcOOB2 = [m.func1OOB_2, m.func2OOB_2 ,m.func3OOB_2, m.func4OOB_2, m.func5OOB_2, m.func6OOB_2];

for (var i = 0; i < funcOOB2.length; i ++)
{
    
    try
    {
        ret = funcOOB2[i]();
        print("Wrong");
        
    } catch(e)
    {
        if (e instanceof RangeError)
        {
            
        }
        else
            print("Wrong");
        
    }
}
print("PASS");