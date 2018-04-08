// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Atomics.wait throws if agent cannot be suspended, CanBlock is false
info: |
  Assuming [[CanBlock]] is false for the main host.

  Atomics.wait( typedArray, index, value, timeout )

  ... (after args validation)
  6. Let B be AgentCanSuspend().
  7. If B is false, throw a TypeError exception.
  ...
features: [Atomics, SharedArrayBuffer, TypedArray, CannotSuspendMainAgent]
---*/

var sab = new SharedArrayBuffer(4);
var int32Array = new Int32Array(sab);
  
assert.throws(TypeError, function() {
  Atomics.wait(int32Array, 0, 0, 0);
});
