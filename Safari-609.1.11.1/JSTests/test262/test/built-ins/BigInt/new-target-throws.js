// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws a TypeError if BigInt is called with a new target
esid: sec-bigint-constructor
info: |
  1. If NewTarget is not undefined, throw a TypeError exception.
  ...
features: [BigInt]
---*/
assert.sameValue(typeof BigInt, 'function');

assert.throws(TypeError, function() {
  new BigInt();
});

assert.throws(TypeError, function() {
  new BigInt({
    valueOf: function() {
      throw new Test262Error("unreachable");
    }
  });
});
