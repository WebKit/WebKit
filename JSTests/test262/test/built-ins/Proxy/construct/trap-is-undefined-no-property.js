// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.14
esid: sec-proxy-object-internal-methods-and-internal-slots-construct-argumentslist-newtarget
description: >
    If the construct trap is not set, propagate the construct to the target object.
info: |
    [[Construct]] (argumentsList, newTarget)

    7. If trap is undefined, then
        b. Return Construct(target, argumentsList, newTarget).
features: [new.target, Proxy, Reflect, Reflect.construct]
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

var P = new Proxy(Target, {});
var obj = Reflect.construct(P, [1, 2], NewTarget);
assert.sameValue(obj.sum, 3, "`construct` trap is missing");
assert.sameValue(calls, 1, "target is called once");
