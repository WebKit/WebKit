// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 9.5.13
esid: sec-proxy-object-internal-methods-and-internal-slots-call-thisargument-argumentslist
description: >
    If the apply trap value is undefined, propagate the call to the target object.
info: |
    [[Call]] (thisArgument, argumentsList)

    ...
    5. Let trap be ? GetMethod(handler, "apply").
    6. If trap is undefined, then
      a. Return ? Call(target, thisArgument, argumentsList).
    ...

    GetMethod ( V, P )

    ...
    3. If func is either undefined or null, return undefined.
    ...
features: [Proxy]
---*/

var calls = 0;

function target(a, b) {
  assert.sameValue(this, ctx);
  calls += 1;
  return a + b;
}

var ctx = {};
var p = new Proxy(target, {
  apply: undefined
});
var res = p.call(ctx, 1, 2);
assert.sameValue(res, 3, "`apply` trap is `null`");
assert.sameValue(calls, 1, "target is called once");
