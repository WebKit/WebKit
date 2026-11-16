//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function func1(c, d, e)
{
    var x;
    var i =0;
    
    if (c == true)
    {
        x = SIMD.Int32x4(1, 2.0, 3, 4);
    }
    else
    {
        x = SIMD.Float32x4(5, 6.5, 7, 8.5);
    }
    
    for (i = 0; i < 10; i++)
    {
        if (d == true)
        {
            return SIMD.Int32x4.add(x, x);
        }
        else
        {
            return SIMD.Float32x4.add(x, x);
        }
    }
    
    
}

var c = true;
var d = true;
var z;
for (i = 0; i < 100; i++)
{
    z = func1(c, d);
    c = !c;
    d = !d;
}
equalSimd([10.0,13.0,14.0,17.0], z, SIMD.Float32x4, "func1");
print("PASS");