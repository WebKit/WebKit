//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    function f1(){
        var bb = -2147483649;
        return +bb;
    }
    
    return { 
        f1 : f1
    };
}
var stdlib = {}
var env = {}
var buffer = new ArrayBuffer(1<<20);
var asmModule = AsmModule(stdlib,env,buffer);
print(asmModule.f1());

var m = function (stdlib, foreign, heap) {
  'use asm';
  function f() {
    return +-137438953473;
  }
  return f;
}(stdlib,env, buffer);
print(m());
