// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.5
description: >
    Trap returns abrupt.
info: |
    [[GetOwnProperty]] (P)

    ...
    9. Let trapResultObj be Call(trap, handler, «target, P»).
    10. ReturnIfAbrupt(trapResultObj).
    ...
features: [Proxy]
---*/

var p = new Proxy({}, {
  getOwnPropertyDescriptor: function(t, prop) {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  Object.getOwnPropertyDescriptor(p, "attr");
});
