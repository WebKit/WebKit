// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.8
description: >
    [[Get]] (P, Receiver)

    14. Return trapResult.
features: [Proxy]
---*/

var target = {};
var p = new Proxy(target, {
  get: function() {
    return 2;
  }
});

Object.defineProperty(target, 'attr', {
  configurable: true,
  writable: false,
  value: 1
});

assert.sameValue(p.attr, 2);
assert.sameValue(p['attr'], 2);
