// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.3
description: >
    Throws a TypeError exception if trap is not callable.
info: |
    [[IsExtensible]] ( )

    ...
    1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
    ...
    5. Let trap be GetMethod(handler, "isExtensible").
    ...
        7.3.9 GetMethod (O, P)
        ...
        2. Let func be GetV(O, P).
        5. If IsCallable(func) is false, throw a TypeError exception.
        ...
features: [Proxy]
---*/


var target = {};
var p = new Proxy(target, {
  isExtensible: {}
});

assert.throws(TypeError, function() {
  Object.isExtensible(p);
});
