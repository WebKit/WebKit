//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var asmHeap = new ArrayBuffer(33554432);
var m = (function(stdlib, foreign, heap) { 'use asm';
  var Uint8ArrayView = new stdlib.Uint8Array(heap);
  function f()
  {
    var i2 = 0;
    (Uint8ArrayView[33554431]) = i2;
    return 0;
  }
  return f; })(this, {}, asmHeap)

m();
m();

var asmHeap = new ArrayBuffer(65536);
var m = (function(stdlib, foreign, heap) { 'use asm';
  var Uint8ArrayView = new stdlib.Uint8Array(heap);
  function f(d0, i1)
  {
    d0 = +d0;
    i1 = i1|0;
    var i2 = 0;
    i2 = 524288;
    (Uint8ArrayView[i2 >> 0]) = i2;
    return ;
  }
  return f; })(this, {}, asmHeap)

m();
m();

WScript.Echo("Passed");