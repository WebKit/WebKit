//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compares the value set by interpreter with the jitted code
// need to run with -mic:1 -off:simplejit -off:JITLoopBody
// Run locally with -trace:memop -trace:bailout to help find bugs

const global = this;
const types = "Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array".split(" ");
const n = 500;
let passed = 1;

function getTest(name) {
  var fn;
  eval(`fn = function memcopy_${name}(a, b, start, end) {for (var i = start; i < end; i++) { b[i] = a[i]; }}`);
  return fn
}

for(let arrType of types) {
  const src = new global[arrType](n);
  const dst = new global[arrType](n);
  for(let i = 0; i < n; ++i) {
    src[i] = i + 0.5;
    dst[i] = 0;
  }
  const test = getTest(arrType);
  const mid = (n / 2)|0;
  test(src, dst, 0, mid);
  test(src, dst, mid, n);
  for(let j = 0; j < n; j++) {
    if(src[j] !== dst[j])
    {
      passed = 0;
      WScript.Echo(types[i] + " " + j + " " + src[j] + " " + dst[j]);
      break;
    }
  }
}

if(passed === 1) {
  WScript.Echo("PASSED");
} else {
  WScript.Echo("FAILED");
}
