// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakset.prototype.add
description: >
  Symbol may not be used as a WeakSet entry
features: [Symbol, WeakSet]
---*/
var weakset = new WeakSet();
var sym = Symbol();

assert.throws(TypeError, function() {
  weakset.add(sym);
});
