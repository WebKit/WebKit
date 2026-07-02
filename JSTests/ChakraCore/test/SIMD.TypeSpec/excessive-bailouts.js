//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
Perf BUG:
Scenario:
LoopBodyJitting happen on second call. We only have partial profiling info, either X value or Y. The other value remaind Undefined because code didn't execute. 
FromVar x and y are hoisted outside the compiled loop because they are used before defined.
Once we enter the jitted loop body, one of the FromVars before the loop starts will fail because X or Y is undefined. And we bailout every time we try to enter the loop. 

Remedy: 
    1. Don't hoist FromVars outside loops if Value can be Undefined/Null.
    2. Always keep Var on merges, if we are in JIT loop body. (This is important for correctness). 
    3. Always keep Var on merges, if value can be undefined/Null.
*/

WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
var globTotal;
function func1(c, d, e)
{
    var x, y;
    var i =0;
    var j = 0;

    if (c == true)
    {
        x = SIMD.Float32x4(1, 1, 1, 1);
        globTotal = x;
    }
    else if (d == false)
    {
        y = SIMD.Int32x4(2, 2, 2, 2);
        globTotal = y;
    }
    
    for (i = 0; i < 10; i++)
    {
        for (j = 0; j < 10; j++)
        {
            if (c == true)
            {
                globTotal = SIMD.Float32x4.add(globTotal, x);
                x = SIMD.Float32x4(1, 1, 1, 1);
            }
            else if (d == false)
            {
                globTotal = SIMD.Int32x4.add(globTotal, y);
                y = SIMD.Int32x4(2, 2, 2, 2);
            }
        }
  
    }
    
    return x;
}

var c = false;
var d = false;
var z;
for (i = 0; i < 100; i++)
{
z = func1(c, d);
if (i % 2 == 0)
{
    equalSimd([202, 202, 202, 202], globTotal, SIMD.Int32x4, "func1");
}
else
{
    equalSimd([101.0,101.0,101.0,101.0], globTotal, SIMD.Float32x4, "func1");
}
c = !c;
d = !d;
}

print("PASS");

