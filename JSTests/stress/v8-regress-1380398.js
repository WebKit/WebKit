//@ requireOptions("--useResizableArrayBuffer=1")
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --harmony-rab-gsab

load("./resources/v8-mjsunit.js", "caller relative");
load("./resources/v8-typedarray-helpers.js", "caller relative");

function test() {
  const ab = new ArrayBuffer(2996, { maxByteLength: 8588995 });
  const dv = new DataView(ab);
  const len = dv.byteLength;
  return len >= 255;
}

// %PrepareFunctionForOptimization(test);
assertTrue(test());
assertTrue(test());
// %OptimizeFunctionOnNextCall(test);
assertTrue(test());
// assertOptimized(test);
