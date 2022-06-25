// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.values
description: >
    Set.prototype.values ( )

    ...
    2. Return CreateSetIterator(S, "value").


    23.2.5.1 CreateSetIterator Abstract Operation

    ...
    2. If S does not have a [[SetData]] internal slot, throw a TypeError exception.
    ...
features: [WeakSet]
---*/

assert.throws(TypeError, function() {
  Set.prototype.values.call(new WeakSet());
});

assert.throws(TypeError, function() {
  var s = new Set();
  s.values.call(new WeakSet());
});
