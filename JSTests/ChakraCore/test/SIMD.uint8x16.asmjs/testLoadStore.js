//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports, buffer) {
    "use asm";
    
    var ui16 = stdlib.SIMD.Uint8x16;
    var ui16check = ui16.check;
    var ui16add = ui16.add;
    var ui16load  = ui16.load;
    var ui16store  = ui16.store;
    var ui16sub = ui16.sub;

    var loopCOUNT = 3;

    var Int8Heap = new stdlib.Int8Array (buffer);    
    var Uint8Heap = new stdlib.Uint8Array (buffer);    

    var Int16Heap = new stdlib.Int16Array(buffer);
    var Uint16Heap = new stdlib.Uint16Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var Uint32Heap = new stdlib.Uint32Array(buffer);
    var Float32Heap = new stdlib.Float32Array(buffer);
    
    var i16 = stdlib.SIMD.Int8x16;
    var i16check = i16.check;
    var i16fu16 = i16.fromUint8x16Bits;    

    function func0()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var st = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var ld = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        
        var t0 = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            st = ui16store(Int8Heap, index >> 0, ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
            ld = ui16load(Int8Heap, index >> 0);
            y = ui16add(st, ld);
            t0 = ui16add(ui16store(Int8Heap, index >> 0, x), ui16load(Int8Heap, index   >> 0));
            t0 = ui16add(t0, y);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(t0));
    }

    function func1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Uint8Heap, index >> 0);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func1OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }

        // No OOB
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Uint8Heap, index >> 0);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }
    
    function func1OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        // OOB
        index = ((0x10000-160) + 1)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Uint8Heap, index >> 0);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int16Heap, index >> 1);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func2OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        // No OOB here
        index = ((0x10000 - 160))|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int16Heap, index >> 1);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func2OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 6)|0;
        // OOB here
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int16Heap, index >> 1);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
    }

    function func3()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func3OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func3OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 32)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Int32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func4()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func4OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func4OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func5()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func5OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func5OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 12)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func6()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func6OOB_1()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
    }

    function func6OOB_2()
    {
        var x = ui16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        var t = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var y = ui16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            ui16store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = ui16load(Float32Heap, index >> 2);
            y = ui16add(y, t);
            index = (index + 16 ) | 0;
        }
        return i16check(i16fu16(y));
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
var m = asmModule(this, {g1:SIMD.Uint8x16(13216, 1024, 28, 108, 55, 3323, 992, 20000)}, buffer);

var ret;

ret = SIMD.Uint8x16.fromInt8x16Bits(m.func0());
equalSimd([4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func1());
//print("func1");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func2());
//print("func3");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func3());
//print("func3");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func4());
//print("func4");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func5());
//print("func5");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");


ret = SIMD.Uint8x16.fromInt8x16Bits(m.func6());
//print("func6");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160], ret, SIMD.Uint8x16, "Test Load Store");
//

var funcOOB1 = [m.func1OOB_1, m.func2OOB_1 ,m.func3OOB_1, m.func4OOB_1, m.func5OOB_1, m.func6OOB_1];
var RESULTS = [
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96),
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96),
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96),
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96),
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96),
    SIMD.Uint8x16(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -126, -116, -106, -96)
];

for (var i = 0; i < funcOOB1.length; i ++)
{
    try
    {
        ret = SIMD.Uint8x16.fromInt8x16Bits(funcOOB1[i]());
        //print("func" + (i+1) + "OOB_1");
        equalSimd(RESULTS[i], ret, SIMD.Uint8x16, "Test Load Store");

    } catch(e)
    {
        print("Wrong");
    }
}

//
var funcOOB2 = [m.func1OOB_2, m.func2OOB_2 ,m.func3OOB_2, m.func4OOB_2, m.func5OOB_2, m.func6OOB_2];

for (var i = 0; i < funcOOB2.length; i ++)
{
    //print("func" + (i+1) + "OOB_2");
    try
    {
        ret = SIMD.Uint8x16.fromInt8x16Bits(funcOOB2[i]());
        print("Wrong");
        
    } catch(e)
    {
        if (e instanceof RangeError) {
            //print("Correct");
        }
        else
            print("Wrong");
        
    }
}
print("PASS");