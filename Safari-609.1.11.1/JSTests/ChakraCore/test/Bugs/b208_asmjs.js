//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function asmModule(stdlib, imports) {
    "use asm";
    
    var f4 = stdlib.SIMD.Float32x4; 
    var f4check = f4.check;
    
    var f4min = f4.min;
    var f4max = f4.max;
    
    var f4sqrt = f4.sqrt;

    var fround = stdlib.Math.fround;

    function func1()
    {
        var x = f4(-1.0,42.0,-1.0,-1.0);
        var y = f4(-1.0,-1.0,-1.0,3.14);
        var res = f4(0.0,0.0,0.0,0.0);
        
        x = f4sqrt(x); //generate nans in the right positions
        y = f4sqrt(y);

        res = f4max(x,y);

        return f4check(res);
    }
    
    function func2()
    {
        var x = f4(-1.0,42.0,-1.0,-1.0);
        var y = f4(-1.0,-1.0,-1.0,3.14);
        var res = f4(0.0,0.0,0.0,0.0);
        
        x = f4sqrt(x); //generate nans in the right positions
        y = f4sqrt(y);

        res = f4min(x,y);

        return f4check(res);
    }
   
    return {func1:func1, func2:func2};
}

var m = asmModule(this, {});

m.func1();
m.func2();

print (m.func1());
print (m.func2());


