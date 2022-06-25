// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-sharedarraybuffer.prototype.grow
description: >
  SharedArrayBuffer.prototype.grow is not a constructor function.
info: |
  SharedArrayBuffer.prototype.grow ( newLength )

  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
features: [SharedArrayBuffer, resizable-arraybuffer]
---*/

assert.sameValue(
  Object.prototype.hasOwnProperty.call(SharedArrayBuffer.prototype.grow, 'prototype'),
  false
);

var arrayBuffer = new SharedArrayBuffer(8);
assert.throws(TypeError, function() {
  new arrayBuffer.grow();
});
