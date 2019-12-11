//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports, buffer) {
    "use asm";
    
    var i8 = stdlib.SIMD.Int16x8;
    var i8check = i8.check;
    var i8add = i8.add;
    var i8load  = i8.load;  
    var i8store  = i8.store
    var i8sub = i8.sub;
    
    var globImporti8 = i8check(imports.g1); 
    var g2 = i8(1065, -1073, -107, 10, 30000, -30000, 9939, -182);  
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
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var st = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var ld = i8(0, 0, 0, 0, 0, 0, 0, 0);
        
        var t0 = i8(0, 0, 0, 0, 0, 0, 0, 0);
        
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            st = i8store(Int8Heap, index >> 0,  i8(1, 2, 3, 4, 5, 6, 7, 8));
            ld = i8load(Int8Heap, index >> 0);
            y = i8add(st, ld);
            t0 = i8add(i8store(Int8Heap, index >> 0, x), i8load(Int8Heap, index   >> 0));
            t0 = i8add(t0, y);
            index = (index + 16 ) | 0;
        }
        return i8check(t0);
    }

    function func1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Uint8Heap, index >> 0);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func1OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }

        // No OOB
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Uint8Heap, index >> 0);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }
    
    function func1OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Int8Heap, index >> 0, x);
            index = (index + 16 ) | 0;
        }
        
        // OOB
        index = ((0x10000-160) + 1)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Uint8Heap, index >> 0);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int16Heap, index >> 1);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func2OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        // No OOB here
        index = ((0x10000 - 160))|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int16Heap, index >> 1);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func2OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 6)|0;
        // OOB here
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int16Heap, index >> 1);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
    }

    function func3()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = 100;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func3OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func3OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint16Heap, index >> 1, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160) + 32)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Int32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func4()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
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

    function func4OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func4OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func5()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
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

    function func5OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func5OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 12)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func6()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 100;
        var size = 10;
        var loopIndex = 0;
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

    function func6OOB_1()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;

        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }

        index = ((0x10000 - 160))|0;
        // No OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
    }

    function func6OOB_2()
    {
        var x = i8(1, 2, 3, 4, 5, 6, 7, 8);
        var t = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var y = i8(0, 0, 0, 0, 0, 0, 0, 0);
        var index = 0;
        var size = 10;
        var loopIndex = 0;
        
        index = (0x10000-160)|0;
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            i8store(Uint32Heap, index >> 2, x);
            index = (index + 16 ) | 0;
        }
        
        index = ((0x10000 - 160) + 16)|0;
        // OOB
        for (loopIndex = 0; (loopIndex | 0) < (size | 0) ; loopIndex = (loopIndex + 1) | 0)
        {
            t = i8load(Float32Heap, index >> 2);
            y = i8add(y, t);
            index = (index + 16 ) | 0;
        }
        return i8check(y);
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
var m = asmModule(this, {g1:SIMD.Int16x8(13216, 1024,-28, -108, 55, -3323, 992, 20000)}, buffer);

var ret;

ret = m.func0();
equalSimd([4, 8, 12, 16, 20, 24, 28, 32], ret, SIMD.Int16x8, "Test Load Store");

ret = m.func1();
//print("func1");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


ret = m.func2();
//print("func3");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


ret = m.func3();
//print("func3");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


ret = m.func4();
//print("func4");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


ret = m.func5();
//print("func5");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


ret = m.func6();
//print("func6");
equalSimd([10, 20, 30, 40, 50, 60, 70, 80], ret, SIMD.Int16x8, "Test Load Store");


//

var funcOOB1 = [m.func1OOB_1, m.func2OOB_1 ,m.func3OOB_1, m.func4OOB_1, m.func5OOB_1, m.func6OOB_1];
var RESULTS = [SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80),
SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80),
SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80),
SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80),
SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80),
SIMD.Int16x8(10, 20, 30, 40, 50, 60, 70, 80)
];

for (var i = 0; i < funcOOB1.length; i ++)
{
    try
    {
        ret = funcOOB1[i]();
        //print("func" + (i+1) + "OOB_1");
        equalSimd(RESULTS[i], ret, SIMD.Int16x8, "Test Load Store");

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
        ret = funcOOB2[i]();
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