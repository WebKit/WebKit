//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
var globTotal;
function increment(a, b, lib)
{
    return lib.add(a, b);
}
function F4Test(b, lib)
{
    var x, y;
    var i =0;
    var j = 0;
    var z;
    x = lib(100,101,102,103);
    y = lib(0, 1,2,3);
   
    if (b == true)
    {
        // swizzle
        z = lib.shuffle(x, y, 0, 0, 0, 0);
        equalSimd([100,100,100,100], z, lib, "Float32x4-1");
        
        
    
        // 2 and 2
        z = lib.shuffle(x, y, 5, 6, 1, 2);
        equalSimd([1.0,2.0,101.0,102.0], z, lib, "Float32x4-2");
        z = lib.shuffle(x, y, 5, 1, 6, 2);
        equalSimd([1.0,101.0,2.0,102.0], z, lib, "Float32x4-3");
        z = lib.shuffle(x, y, 1, 5, 2, 6);
        equalSimd([101.0,1.0,102.0,2.0], z, lib, "Float32x4-4");
    }
    else
    {
        // 3 and 1
        z = lib.shuffle(x, y, 1, 5, 6, 7);
        equalSimd([101.0,1.0,2.0,3.0], z, lib, "Float32x4-5");
        z = lib.shuffle(x, y, 5, 1, 6, 7);
        equalSimd([1.0,101.0,2.0,3.0], z, lib, "Float32x4-6");
        z = lib.shuffle(x, y, 5, 1, 3, 2);
        equalSimd([1.0,101.0,103.0,102.0], z, lib, "Float32x4-7");
        z = lib.shuffle(x, y, 1, 5, 0, 1);
        equalSimd([101.0,1.0,100.0,101.0], z, lib, "Float32x4-8");
    }
    if (lib === SIMD.Float32x4)
    {
        return lib.swizzle(lib.abs(lib.sub(y, z)), 1, 2, 0, 3);
    }
    else
    {
        return lib.swizzle(lib.add(z, y), 3, 2, 0, 1);
    }
}

function I4Test(b, lib)
{
    var x, y;
    var i =0;
    var j = 0;
    var z;
    x = lib(100,101,102,103);
    y = lib(0, 1,2,3);
   
    if (b == true)
    {
        // swizzle
        z = lib.shuffle(x, y, 0, 0, 0, 0);
        equalSimd([100,100,100,100], z, lib, "Float32x4-1");
    
        // 2 and 2
        z = lib.shuffle(x, y, 5, 6, 1, 2);
        equalSimd([1.0,2.0,101.0,102.0], z, lib, "Float32x4-2");
        z = lib.shuffle(x, y, 5, 1, 6, 2);
        equalSimd([1.0,101.0,2.0,102.0], z, lib, "Float32x4-3");
        z = lib.shuffle(x, y, 1, 5, 2, 6);
        equalSimd([101.0,1.0,102.0,2.0], z, lib, "Float32x4-4");
    }
    else
    {
        // 3 and 1
        z = lib.shuffle(x, y, 1, 5, 6, 7);
        equalSimd([101.0,1.0,2.0,3.0], z, lib, "Float32x4-5");
        z = lib.shuffle(x, y, 5, 1, 6, 7);
        equalSimd([1.0,101.0,2.0,3.0], z, lib, "Float32x4-6");
        z = lib.shuffle(x, y, 5, 1, 3, 2);
        equalSimd([1.0,101.0,103.0,102.0], z, lib, "Float32x4-7");
        z = lib.shuffle(x, y, 1, 5, 0, 1);
        equalSimd([101.0,1.0,100.0,101.0], z, lib, "Float32x4-8");
    }
    if (lib === SIMD.Float32x4)
    {
        return lib.swizzle(lib.abs(lib.sub(y, z)), 1, 2, 0, 3);
    }
    else
    {
        return lib.swizzle(lib.add(z, y), 3, 2, 0, 1);
    }
}

var c = false;
var d = false;
var z;
var lib;

lib = SIMD.Float32x4;
z = F4Test(true, lib);
equalSimd([0, 100, 101, 1], z, lib, "F4Test-true");
z = F4Test(false, lib);
equalSimd([0, 98, 101, 98], z, lib, "F4Test-true");
z = F4Test(true, lib);
equalSimd([0, 100, 101, 1], z, lib, "F4Test-true");
z = F4Test(false, lib);
equalSimd([0, 98, 101, 98], z, lib, "F4Test-true");

// This will make the calls polymorphic, we don't inline and call helpers instead
lib = SIMD.Int32x4;
z = I4Test(true, lib);
equalSimd([5, 104, 101, 2], z, lib, "I4Test-true");
z = I4Test(false, lib);
equalSimd([104, 102, 101, 2], z, lib, "I4Test-true");
z = I4Test(true, lib);
equalSimd([5, 104, 101, 2], z, lib, "I4Test-true");
z = I4Test(false, lib);
equalSimd([104, 102, 101, 2], z, lib, "I4Test-true");


print("PASS");
