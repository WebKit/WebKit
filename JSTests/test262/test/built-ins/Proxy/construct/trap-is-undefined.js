// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 9.5.14
esid: sec-proxy-object-internal-methods-and-internal-slots-construct-argumentslist-newtarget
description: >
    If the construct trap value is undefined, propagate the construct to the target object.
info: |
    [[Construct]] (argumentsList, newTarget)

    ...
    5. Let trap be ? GetMethod(handler, "construct").
    6. If trap is undefined, then
      a. Assert: target has a [[Construct]] internal method.
      b. Return ? Construct(target, argumentsList, newTarget).
    ...

    GetMethod ( V, P )

    ...
    3. If func is either undefined or null, return undefined.
    ...
features: [Reflect.construct]
---*/

var calls = 0;

function NewTarget() {}

function Target(a, b) {
  assert.sameValue(new.target, NewTarget);
  calls += 1;
  return {
    sum: a + b
  };
}

var P = new Proxy(Target, {
  construct: undefined
});
var obj = Reflect.construct(P, [3, 4], NewTarget);
assert.sameValue(obj.sum, 7, "`construct` trap is `undefined`");
assert.sameValue(calls, 1, "target is called once");
