//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    var HEAP32 =new stdlib.Float32Array(buffer);
    var fround = stdlib.Math.fround;
    var c = foreign.fun2;
    function f() {
        var a = fround(0);
        var b = 0.;
        a = fround(HEAP32[0]);
        b = +a;
        c(b);
        return (~~b)|0;
    }
    
    return {
        f : f
    };
}

var global = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {fun1:function(x1,x2,x3,x4,x5,x6,x7,x8){print(x1,x2,x3,x4,x5,x6,x7,x8);}, fun2:function(x){print(x);},x:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer = new ArrayBuffer(1<<20);
var view = new Int32Array(buffer);
view[0] = 0xffffffff
var asmModule = new AsmModule(global,env,buffer);

print(asmModule.f(Number.MAX_VALUE));
print(asmModule.f(Number.MAX_VALUE));